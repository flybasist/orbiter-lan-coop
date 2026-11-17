#include "scenario_launch.h"

#include <commctrl.h>

#include <cstring>
#include <fstream>
#include <string>

#include "scenario_filter.h"

namespace olcplugin {

namespace {

// Launchpad element numbers (Src/Orbiter/resource.h of the base commit).
constexpr int kLaunchButtonId = 1000;  // IDLAUNCH
constexpr int kScenarioListId = 1090;  // IDC_SCN_LIST

// The file and its place in the list: the folder OLC in Scenarios, the item "Host world".
const char kFolder[] = "OLC";
const char kItem[] = "Host world";
const char kFolderPath[] = "Scenarios\\OLC";
const char kFilePath[] = "Scenarios\\OLC\\Host world.scn";

// Looks for the scenario list among all child windows of Launchpad (it is on the tab page).
// The number alone is not enough: another tab has a check box with the same number.
BOOL CALLBACK findList(HWND window, LPARAM result) {
    char className[64] = {};
    GetClassNameA(window, className, sizeof(className));
    if (GetDlgCtrlID(window) == kScenarioListId && std::strcmp(className, WC_TREEVIEWA) == 0) {
        *reinterpret_cast<HWND*>(result) = window;
        return FALSE;  // found, stop
    }
    return TRUE;
}

// The child item with the given text; nullptr - none. parent TVI_ROOT - top level.
HTREEITEM findItem(HWND list, HTREEITEM parent, const char* text) {
    HTREEITEM item = parent == TVI_ROOT ? TreeView_GetRoot(list) : TreeView_GetChild(list, parent);
    while (item) {
        char buffer[256] = {};
        TVITEMA data = {};
        data.mask = TVIF_TEXT;
        data.hItem = item;
        data.pszText = buffer;
        data.cchTextMax = sizeof(buffer);
        if (TreeView_GetItem(list, &data) && std::strcmp(buffer, text) == 0) {
            return item;
        }
        item = TreeView_GetNextSibling(list, item);
    }
    return nullptr;
}

// Adds an item. Launchpad tells a folder from a file by "has children" (TabScenario.cpp,
// GetSelScenario), so the folder gets cChildren = 1 and the file 0.
HTREEITEM addItem(HWND list, HTREEITEM parent, const char* text, bool folder) {
    TVINSERTSTRUCTA insert = {};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_SORT;
    insert.item.mask = TVIF_TEXT | TVIF_CHILDREN;
    insert.item.pszText = const_cast<char*>(text);
    insert.item.cChildren = folder ? 1 : 0;
    return reinterpret_cast<HTREEITEM>(
        SendMessageA(list, TVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&insert)));
}

}  // namespace

bool classInstalled(const std::string& className) {
    std::string path = "Vessels\\" + className + ".cfg";
    FILEHANDLE file = oapiOpenFile(path.c_str(), FILE_IN_ZEROONFAIL, CONFIG);
    if (!file) {
        return false;
    }
    oapiCloseFile(file, FILE_IN_ZEROONFAIL);
    return true;
}

bool writeReceivedScenario(const std::vector<uint8_t>& data, std::string& error) {
    const olc::ScenarioFilterResult filtered =
        olc::filterScenario(std::string(data.begin(), data.end()), classInstalled);
    std::string classes;
    for (const std::string& className : filtered.missingClasses) {
        classes += (classes.empty() ? "" : ", ") + className;
    }
    for (const std::string& name : filtered.removedVessels) {
        oapiWriteLogV("OLC client: vessel '%s' left out, its class is not installed here",
                      name.c_str());
    }
    if (!filtered.hasVessels) {
        error = "None of the host's vessel classes is installed here: " + classes + ".";
        oapiWriteLogV("OLC client: %s", error.c_str());
        return false;
    }

    CreateDirectoryA(kFolderPath, nullptr);  // an existing folder is not an error here
    std::ofstream file(kFilePath, std::ios::binary);
    file << filtered.text;
    if (!file) {
        error = std::string("Cannot write ") + kFilePath + ".";
        oapiWriteLogV("OLC client: cannot write %s", kFilePath);
        return false;
    }
    return true;
}

bool launchReceivedScenario(HWND launchpad) {
    HWND list = nullptr;
    EnumChildWindows(launchpad, findList, reinterpret_cast<LPARAM>(&list));
    if (!list) {
        oapiWriteLogV("OLC client: scenario list not found in Launchpad");
        return false;
    }
    // Launchpad fills the list when it is shown, so a file written later is not in it yet.
    HTREEITEM folder = findItem(list, TVI_ROOT, kFolder);
    if (!folder) {
        folder = addItem(list, TVI_ROOT, kFolder, true);
    }
    HTREEITEM item = folder ? findItem(list, folder, kItem) : nullptr;
    if (folder && !item) {
        item = addItem(list, folder, kItem, false);
    }
    if (!item) {
        oapiWriteLogV("OLC client: cannot add the scenario to the Launchpad list");
        return false;
    }
    TreeView_Expand(list, folder, TVE_EXPAND);
    TreeView_SelectItem(list, item);
    if (TreeView_GetSelection(list) != item) {
        oapiWriteLogV("OLC client: cannot select the scenario in the Launchpad list");
        return false;
    }
    // The same as pressing "Launch": the button takes the selected scenario.
    PostMessageA(launchpad, WM_COMMAND, kLaunchButtonId, 0);
    oapiWriteLogV("OLC client: starting the host's scenario %s", kFilePath);
    return true;
}

}  // namespace olcplugin
