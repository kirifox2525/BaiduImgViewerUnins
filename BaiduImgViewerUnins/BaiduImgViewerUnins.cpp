#include "framework.h"
#include "BaiduImgViewerUnins.h"
#include <shellapi.h>
#include <commctrl.h>
#include <string>
#ifndef TDCBF_OK_BUTTON
typedef int TASKDIALOG_COMMON_BUTTON_FLAGS;
#define TDCBF_OK_BUTTON 0x0001
#define TDCBF_YES_BUTTON 0x0002
#define TDCBF_NO_BUTTON 0x0004
#define TD_ERROR_ICON MAKEINTRESOURCEW(-2)
#define TD_INFORMATION_ICON MAKEINTRESOURCEW(-3)
#endif
#pragma comment(lib,"Advapi32.lib")
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define IDC_LOG 1002
#define WM_START_CHECK (WM_APP+1)
HINSTANCE app; HWND out; HFONT font; HANDLE logFile=INVALID_HANDLE_VALUE; std::wstring logPath;
typedef HRESULT (WINAPI* TaskDialogProc)(HWND,HINSTANCE,PCWSTR,PCWSTR,PCWSTR,TASKDIALOG_COMMON_BUTTON_FLAGS,PCWSTR,int*);
int modernDialog(const wchar_t* title,const wchar_t* heading,const wchar_t* text,TASKDIALOG_COMMON_BUTTON_FLAGS buttons,PCWSTR icon)
{
    HMODULE module=LoadLibraryW(L"comctl32.dll");
    TaskDialogProc taskDialog=module?reinterpret_cast<TaskDialogProc>(GetProcAddress(module,"TaskDialog")):nullptr;
    int result=IDCANCEL;
    if(taskDialog) taskDialog(nullptr,app,title,heading,text,buttons,icon,&result);
    else {
        std::wstring message=std::wstring(heading)+L"\n\n"+text;
        UINT flags=(buttons&TDCBF_YES_BUTTON)?(MB_YESNO|MB_DEFBUTTON2):MB_OK;
        flags|=(icon==TD_ERROR_ICON)?MB_ICONERROR:MB_ICONINFORMATION;
        result=MessageBoxW(nullptr,message.c_str(),title,flags);
    }
    if(module) FreeLibrary(module);
    return result;
}

std::wstring componentPath(){wchar_t p[MAX_PATH]{};GetEnvironmentVariableW(L"APPDATA",p,MAX_PATH);return std::wstring(p)+L"\\baidu\\BaiduNetdisk\\module\\ImageViewer";}
bool keyExists(HKEY root,const wchar_t* sub){HKEY k{};LONG r=RegOpenKeyExW(root,sub,0,KEY_READ,&k);if(r==ERROR_SUCCESS)RegCloseKey(k);return r==ERROR_SUCCESS;}
bool valueExists(){HKEY k{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\RegisteredApplications",0,KEY_QUERY_VALUE,&k)!=ERROR_SUCCESS)return false;LONG r=RegQueryValueExW(k,L"BaiduNetdiskImageViewer",nullptr,nullptr,nullptr,nullptr);RegCloseKey(k);return r==ERROR_SUCCESS;}
bool dirExists(){DWORD a=GetFileAttributesW(componentPath().c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);}
bool installed(){return keyExists(HKEY_CLASSES_ROOT,L"BaiduNetdiskImageViewerAssociations")||keyExists(HKEY_CURRENT_USER,L"Software\\Baidu\\BaiduNetdiskImageViewer")||valueExists()||dirExists();}

void writeUtf8(const std::wstring&s){if(logFile==INVALID_HANDLE_VALUE)return;int n=WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,nullptr,0,nullptr,nullptr);std::string b(n-1,'\0');WideCharToMultiByte(CP_UTF8,0,s.c_str(),-1,b.data(),n,nullptr,nullptr);b+="\r\n";DWORD written;WriteFile(logFile,b.data(),(DWORD)b.size(),&written,nullptr);FlushFileBuffers(logFile);}
void log(const std::wstring&s)
{
    SYSTEMTIME t{}; GetLocalTime(&t);
    wchar_t stamp[32]{};
    swprintf_s(stamp,L"[%04u-%02u-%02u %02u:%02u:%02u] ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
    std::wstring line=stamp+s;
    writeUtf8(line);
    if(out){int n=GetWindowTextLengthW(out);SendMessageW(out,EM_SETSEL,n,n);std::wstring x=line+L"\r\n";SendMessageW(out,EM_REPLACESEL,0,(LPARAM)x.c_str());}
}
void openLog(){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);std::wstring dir=exe;dir.resize(dir.find_last_of(L"\\/")+1);SYSTEMTIME t{};GetLocalTime(&t);wchar_t name[64];swprintf_s(name,L"Unins-%04u%02u%02u-%02u%02u%02u.log",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);logPath=dir+name;logFile=CreateFileW(logPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(logFile!=INVALID_HANDLE_VALUE){DWORD w;const BYTE bom[]={0xEF,0xBB,0xBF};WriteFile(logFile,bom,3,&w,nullptr);}}

LONG deleteTreeXp(HKEY root,const wchar_t* sub)
{
    HKEY key{}; LONG r=RegOpenKeyExW(root,sub,0,KEY_READ|KEY_WRITE,&key);
    if(r!=ERROR_SUCCESS)return r;
    wchar_t child[256]; DWORD size=256;
    while((r=RegEnumKeyExW(key,0,child,&size,nullptr,nullptr,nullptr,nullptr))==ERROR_SUCCESS){
        LONG childResult=deleteTreeXp(key,child);
        if(childResult!=ERROR_SUCCESS&&childResult!=ERROR_FILE_NOT_FOUND){RegCloseKey(key);return childResult;}
        size=256;
    }
    RegCloseKey(key);
    if(r!=ERROR_NO_MORE_ITEMS)return r;
    return RegDeleteKeyW(root,sub);
}
void delKey(HKEY h,const wchar_t*k,const wchar_t*n){LONG r=deleteTreeXp(h,k);log(std::wstring(r==ERROR_SUCCESS?L"[SUCCESS] Deleted ":r==ERROR_FILE_NOT_FOUND?L"[SKIPPED] Not found: ":L"[FAILED] Could not delete ")+n+(r!=ERROR_SUCCESS&&r!=ERROR_FILE_NOT_FOUND?L", error code "+std::to_wstring(r):L""));}
void delValue(){HKEY k{};LONG r=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\RegisteredApplications",0,KEY_SET_VALUE,&k);if(r==ERROR_SUCCESS){r=RegDeleteValueW(k,L"BaiduNetdiskImageViewer");RegCloseKey(k);}log(std::wstring(r==ERROR_SUCCESS?L"[SUCCESS] Deleted ":r==ERROR_FILE_NOT_FOUND?L"[SKIPPED] Not found: ":L"[FAILED] Could not delete ")+L"RegisteredApplications\\BaiduNetdiskImageViewer"+(r!=ERROR_SUCCESS&&r!=ERROR_FILE_NOT_FOUND?L", error code "+std::to_wstring(r):L""));}
void delFiles(){auto p=componentPath();if(!dirExists()){log(L"[SKIPPED] Directory not found: "+p);return;}auto q=p;q.append(2,L'\0');SHFILEOPSTRUCTW s{};s.wFunc=FO_DELETE;s.pFrom=q.c_str();s.fFlags=FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_SILENT;int r=SHFileOperationW(&s);log(r==0&&!s.fAnyOperationsAborted?L"[SUCCESS] Deleted directory: "+p:L"[FAILED] Could not delete directory, error code "+std::to_wstring(r));}
void reportState(){log(std::wstring(L"Detection: file association=")+(keyExists(HKEY_CLASSES_ROOT,L"BaiduNetdiskImageViewerAssociations")?L"present":L"absent")+L", user configuration="+(keyExists(HKEY_CURRENT_USER,L"Software\\Baidu\\BaiduNetdiskImageViewer")?L"present":L"absent")+L", registered application value="+(valueExists()?L"present":L"absent")+L", component directory="+(dirExists()?L"present":L"absent"));}

void workflow(HWND w){log(L"Starting Smart Image Viewer detection...");reportState();if(!installed()){log(L"Smart Image Viewer is not installed; no action is required.");modernDialog(L"卸载“智能看图”",L"无需操作",L"无需进行“智能看图”卸载",TDCBF_OK_BUTTON,TD_INFORMATION_ICON);return;}if(modernDialog(L"卸载“智能看图”",L"发现智能看图",L"是否继续卸载“智能看图”？",TDCBF_YES_BUTTON|TDCBF_NO_BUTTON,TD_INFORMATION_ICON)!=IDYES){log(L"The user cancelled the uninstall operation.");return;}for(int attempt=1;attempt<=3;++attempt){log(L"===== Cleanup attempt "+std::to_wstring(attempt)+L"/3 =====");delKey(HKEY_CLASSES_ROOT,L"BaiduNetdiskImageViewerAssociations",L"file association");delKey(HKEY_CURRENT_USER,L"Software\\Baidu\\BaiduNetdiskImageViewer",L"user configuration");delValue();delFiles();reportState();if(!installed()){log(L"Smart Image Viewer was uninstalled successfully.");modernDialog(L"卸载“智能看图”",L"卸载完成",L"已完成卸载“智能看图”",TDCBF_OK_BUTTON,TD_INFORMATION_ICON);return;}if(attempt<3){log(L"Remaining items were detected; preparing another attempt.");Sleep(500);}}log(L"Maximum retry count reached; uninstall remnants still exist.");modernDialog(L"卸载“智能看图”",L"遇到错误",L"无法完成卸载“智能看图”\n查看日志文件以了解详细信息",TDCBF_OK_BUTTON,TD_ERROR_ICON);}

int APIENTRY wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int)
{
    app=h;
    openLog();
    workflow(nullptr);
    if(logFile!=INVALID_HANDLE_VALUE){CloseHandle(logFile);logFile=INVALID_HANDLE_VALUE;}
    return 0;
}