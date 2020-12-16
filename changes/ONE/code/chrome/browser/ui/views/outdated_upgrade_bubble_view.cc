// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/installer/util/google_update_util.h"
#endif

namespace {

// The URL to be used to re-install Chrome when auto-update failed for too long.
constexpr char kDownloadChromeUrl[] =
    "https://www.google.com/chrome/?&brand=CHWL"
    "&utm_campaign=en&utm_source=en-et-na-us-chrome-bubble&utm_medium=et";

// The maximum number of ignored bubble we track in the NumLaterPerReinstall
// histogram.
constexpr int kMaxIgnored = 50;
// The number of buckets we want the NumLaterPerReinstall histogram to use.
constexpr int kNumIgnoredBuckets = 5;

class OutdatedUpgradeBubbleDelegate;
// The currently showing bubble.
OutdatedUpgradeBubbleDelegate* g_upgrade_bubble = nullptr;

// The number of times the user ignored the bubble before finally choosing to
// reinstall.
int g_num_ignored_bubbles = 0;

class OutdatedUpgradeBubbleDelegate : public ui::DialogModelDelegate {
 public:
  OutdatedUpgradeBubbleDelegate(content::PageNavigator* navigator,
                                bool auto_update_enabled)
      : auto_update_enabled_(auto_update_enabled), navigator_(navigator) {}

  OutdatedUpgradeBubbleDelegate(const OutdatedUpgradeBubbleDelegate&) = delete;
  OutdatedUpgradeBubbleDelegate& operator=(
      const OutdatedUpgradeBubbleDelegate&) = delete;

  ~OutdatedUpgradeBubbleDelegate() override {
    // Increment the ignored bubble count (if this bubble wasn't ignored, this
    // increment is offset by a decrement in OnDialogAccepted()).
    if (g_num_ignored_bubbles < kMaxIgnored)
      ++g_num_ignored_bubbles;
  }

  void OnWindowClosing() {
    // Reset |g_upgrade_bubble| here, not in destructor, because destruction is
    // asynchronous and ShowBubble may be called before full destruction and
    // would attempt to show a bubble that is closing.
    DCHECK_EQ(g_upgrade_bubble, this);
    g_upgrade_bubble = nullptr;
  }

  void OnDialogAccepted() {
    // Offset the +1 in the dtor.
    --g_num_ignored_bubbles;
    if (auto_update_enabled_) {
      DCHECK(UpgradeDetector::GetInstance()->is_outdated_install());
      UMA_HISTOGRAM_CUSTOM_COUNTS("OutdatedUpgradeBubble.NumLaterPerReinstall",
                                  g_num_ignored_bubbles, 1, kMaxIgnored,
                                  kNumIgnoredBuckets);
      base::RecordAction(
          base::UserMetricsAction("OutdatedUpgradeBubble.Reinstall"));
      navigator_->OpenURL(
          content::OpenURLParams(GURL(kDownloadChromeUrl), content::Referrer(),
                                 WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                 ui::PAGE_TRANSITION_LINK, false));
#if defined(OS_WIN)
    } else {
      DCHECK(UpgradeDetector::GetInstance()->is_outdated_install_no_au());
      UMA_HISTOGRAM_CUSTOM_COUNTS("OutdatedUpgradeBubble.NumLaterPerEnableAU",
                                  g_num_ignored_bubbles, 1, kMaxIgnored,
                                  kNumIgnoredBuckets);
      base::RecordAction(
          base::UserMetricsAction("OutdatedUpgradeBubble.EnableAU"));
      // Record that the autoupdate flavour of the dialog has been shown.
      if (g_browser_process->local_state()) {
        g_browser_process->local_state()->SetBoolean(
            prefs::kAttemptedToEnableAutoupdate, true);
      }

      // Re-enable updates by shelling out to setup.exe asynchronously.
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&google_update::ElevateIfNeededToReenableUpdates));
#endif  // defined(OS_WIN)
    }
  }

 private:
  // Identifies if auto-update is enabled or not.
  const bool auto_update_enabled_;

  // The PageNavigator to use for opening the Download Chrome URL.
  content::PageNavigator* const navigator_;
};

}  // namespace

// OutdatedUpgradeBubbleView ---------------------------------------------------

// static
void OutdatedUpgradeBubbleView::ShowBubble(views::View* anchor_view,
                                           content::PageNavigator* navigator,
                                           bool auto_update_enabled) {
  int a = 0; if (a==0) return;//Thuan.Not show outdated bubble.
  if (g_upgrade_bubble)
    return;

  auto delegate = std::make_unique<OutdatedUpgradeBubbleDelegate>(
      navigator, auto_update_enabled);
  g_upgrade_bubble = delegate.get();

  auto dialog_model =
      ui::DialogModel::Builder(std::move(std::move(delegate)))
          .SetShowCloseButton(true)
          .SetTitle(l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_TITLE))
          .AddOkButton(
              base::BindOnce(&OutdatedUpgradeBubbleDelegate::OnDialogAccepted,
                             base::Unretained(g_upgrade_bubble)),
              l10n_util::GetStringUTF16(auto_update_enabled
                                            ? IDS_REINSTALL_APP
                                            : IDS_REENABLE_UPDATES))
          .AddBodyText(
              ui::DialogModelLabel(IDS_UPGRADE_BUBBLE_TEXT).set_is_secondary())
          .SetWindowClosingCallback(
              base::BindOnce(&OutdatedUpgradeBubbleDelegate::OnWindowClosing,
                             base::Unretained(g_upgrade_bubble)))
          .SetCloseCallback(base::BindOnce(
              &base::RecordAction,
              base::UserMetricsAction("OutdatedUpgradeBubble.Later")))
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  views::BubbleDialogDelegateView::CreateBubble(std::move(bubble))->Show();

  chrome::RecordDialogCreation(chrome::DialogIdentifier::OUTDATED_UPGRADE);

  base::RecordAction(
      auto_update_enabled
          ? base::UserMetricsAction("OutdatedUpgradeBubble.Show")
          : base::UserMetricsAction("OutdatedUpgradeBubble.ShowNoAU"));
}
