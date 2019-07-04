
"C:\Program Files (x86)\WiX Toolset v3.11\bin\heat" dir "O:\sandbox\utility\chromium\src\install\one75_x86" -cg C_MAINDIR -dr MAIN_DIR -srd -var var.MainDir -ag -sfrag -out mainfolder.wxs
rem "C:\Program Files (x86)\WiX Toolset v3.11\bin\heat" dir "..\Bin\template" -cg C_TEMPLATE -dr TEMPLATE_DIR -srd -var var.TemplateDir -ag -sfrag -out template.wxs
rem "C:\Program Files (x86)\WiX Toolset v3.11\bin\heat" dir "..\Bin\storage" -cg C_STORAGE -dr STORAGE_DIR -srd -var var.StorageDir -ag -sfrag -out storage.wxs