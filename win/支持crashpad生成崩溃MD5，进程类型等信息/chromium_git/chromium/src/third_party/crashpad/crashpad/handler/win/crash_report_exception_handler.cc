// Copyright 2015 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "handler/win/crash_report_exception_handler.h"

#include <type_traits>
#include <utility>

#include "client/crash_report_database.h"
#include "client/settings.h"
#include "handler/crash_report_upload_thread.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_user_extension_stream_data_source.h"
#include "snapshot/win/process_snapshot_win.h"
#include "util/file/file_writer.h"
#include "util/misc/metrics.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/scoped_process_suspend.h"
#include "util/win/termination_codes.h"
// wangbo start crashpad unique hash
#include <fstream>
#include "base/hash/md5.h"
#include "include/cef_parser.h"
// wangbo end

namespace crashpad {

// wangbo start crashpad unique hash
#include <DbgHelp.h>
#include <Psapi.h>
#include <windows.h>
#include <winnt.h>
#include <shlwapi.h>
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "Shlwapi.lib")

#define MAKEDWORD(a, b) ((DWORD)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))

BOOL GetCodeLowHigh(HANDLE hProcess,
                    PVOID addr,
                    DWORD& codeHigh,
                    DWORD& codeLow,
                    PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine) {
  IMAGE_DOS_HEADER idh;
  IMAGE_NT_HEADERS inh;
  IMAGE_SECTION_HEADER emptySection;
  memset((void*)&emptySection, 0, sizeof(IMAGE_SECTION_HEADER));
  IMAGE_SECTION_HEADER ish;
  const DWORD dwMask = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE;

  DWORD readed;
  PBYTE ptr = (PBYTE)addr;

  if (ReadMemoryRoutine(hProcess, (DWORD)ptr, &idh, sizeof(idh), &readed) &&
      readed == sizeof(idh) && idh.e_magic == IMAGE_DOS_SIGNATURE &&
      ReadMemoryRoutine(
          hProcess, (DWORD)ptr + idh.e_lfanew, &inh, sizeof(inh), &readed) &&
      readed == sizeof(inh) && inh.Signature == IMAGE_NT_SIGNATURE &&
      inh.FileHeader.Machine == IMAGE_FILE_MACHINE_I386 &&
      inh.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR_MAGIC) {
    ptr = ptr + idh.e_lfanew + sizeof(IMAGE_NT_HEADERS);
    DWORD dwSecCount = inh.FileHeader.NumberOfSections;
    for (DWORD dw = 0; dw < dwSecCount; ++dw) {
      if (ReadMemoryRoutine(hProcess, (DWORD)ptr, &ish, sizeof(ish), &readed) &&
          readed == sizeof(ish)) {
        ptr += sizeof(IMAGE_SECTION_HEADER);
        if (memcmp(&ish, &emptySection, sizeof(IMAGE_SECTION_HEADER)) == 0)
          break;
        if ((ish.Characteristics & dwMask) == dwMask) {
          codeLow = (DWORD)addr + ish.VirtualAddress;
          codeHigh = codeLow + ish.SizeOfRawData;
          return TRUE;
        }
      } else {
        break;
      }
    }
  }
  return FALSE;
}

#define LBIT(b) ((b >> 6) & 0x03)  // get the 6-7 bit of a byte
#define MBIT(b) ((b >> 3) & 0x07)  // get the 5-3 bit of a byte
#define RBIT(b) (b & 0x07)  // get the 2-0 bit of a byte

bool IsCallInstruction(PBYTE pIns, int len) {
  if (len < 2)
    return false;
  if (pIns[0] == 0xe8 && len == 5)
    return true;
  if (pIns[0] != 0xff)
    return false;

  unsigned int mod = LBIT(pIns[1]);
  unsigned int r = MBIT(pIns[1]);
  unsigned int m = RBIT(pIns[1]);
  if (r != 0x02)
    return false;
  if (mod == 3)
    return len == 2;
  if (mod == 0 && m == 5) {
    return len == 6;
  }
  int iLen = 2;
  if (m == 4) {
    ++iLen;
    if (len < 3)
      return false;
    BYTE sib = pIns[2];
    unsigned int b = LBIT(sib);
    if (b == 5 && mod == 0) {
      iLen += 4;
    }
  }
  if (mod == 0)
    iLen += 0;
  if (mod == 1)
    iLen += 1;
  if (mod == 2)
    iLen += 4;
  return (len == iLen);
}

BOOL IsReturnAddr(DWORD addr,
                  HANDLE hProcess,
                  PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine) {
  if ((addr & 0xffff0000) == 0)
    return FALSE;

  MEMORY_BASIC_INFORMATION mbi;

  if (VirtualQueryEx(hProcess, (PVOID)(addr & 0xffff0000), &mbi, sizeof(mbi)) ==
          sizeof(mbi) && mbi.AllocationBase != 0) {
    BYTE code[7];
    DWORD readed;
    if (ReadMemoryRoutine(hProcess, (DWORD)(addr - 7), code, 7, &readed) &&
        readed == 7) {
      for (int i = 0; i < 6; ++i) {
        int len = 7 - i;
        if (IsCallInstruction(code + i, len))
          return TRUE;
      }
      return FALSE;
    }
  }

  return FALSE;
}

DWORD SearchRecord(DWORD dwEsp,
                   DWORD dwStachHigh,
                   HANDLE hProcess,
                   PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine) {
  dwEsp &= ~3;
  for (; dwEsp + 8 <= dwStachHigh; dwEsp += 4) {
    DWORD raddr;
    DWORD readed;
    if (ReadMemoryRoutine(hProcess, (DWORD)(dwEsp + 4), &raddr, 4, &readed) &&
        readed == 4) {
      if (IsReturnAddr(raddr, hProcess, ReadMemoryRoutine)) {
        return dwEsp;
      }
    }
  }
  return 0;
}

extern "C" BOOL WINAPI
MyStackWalk(DWORD MachineType,
            HANDLE hProcess,
            HANDLE hThread,
            LPSTACKFRAME StackFrame,
            LPVOID ContextRecord,
            PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,
            PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine,
            PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine,
            PTRANSLATE_ADDRESS_ROUTINE TranslateAddress,
            int nSearchMode) {
  if (nSearchMode == 0) {
    return StackWalk(MachineType,
                     hProcess,
                     hThread,
                     StackFrame,
                     ContextRecord,
                     ReadMemoryRoutine,
                     FunctionTableAccessRoutine,
                     GetModuleBaseRoutine,
                     TranslateAddress);
  }

  LDT_ENTRY ldtEntry;
  DWORD buf[3];
  DWORD readed;
  DWORD dwStackLow, dwStackHigh;

  if (MachineType != IMAGE_FILE_MACHINE_I386)
    return FALSE;
  if (!ContextRecord)
    return FALSE;
  if (!ReadMemoryRoutine) {
    ReadMemoryRoutine = (PREAD_PROCESS_MEMORY_ROUTINE)ReadProcessMemory;
  }
  if (StackFrame->AddrFrame.Mode != AddrModeFlat)
    return FALSE;

  CONTEXT& ctx = *(CONTEXT*)ContextRecord;
  if (!GetThreadSelectorEntry(hThread, (WORD)ctx.SegFs, &ldtEntry)) {
    return FALSE;
  }
  DWORD base = MAKEDWORD(ldtEntry.BaseLow,
                         MAKEWORD(ldtEntry.HighWord.Bytes.BaseMid,
                                  ldtEntry.HighWord.Bytes.BaseHi));
  if (!(ReadMemoryRoutine(hProcess, base, buf, sizeof(buf), &readed) &&
        readed == sizeof(buf))) {
    return FALSE;
  }
  dwStackHigh = buf[1];
  dwStackLow = buf[2];

  StackFrame->AddrPC.Mode = AddrModeFlat;
  StackFrame->AddrReturn.Mode = AddrModeFlat;
  StackFrame->Far = FALSE;

  if (StackFrame->Virtual == 0) {
    StackFrame->Reserved[0] = StackFrame->Reserved[1] =
        StackFrame->Reserved[2] = 0;
    StackFrame->Virtual = 1;

    DWORD _newebp = StackFrame->AddrFrame.Offset;

    if (_newebp < dwStackLow || _newebp > dwStackHigh) {
      _newebp = 0;
    }

    if (_newebp == 0 || nSearchMode == 3) {
      DWORD Esp = reinterpret_cast<CONTEXT*>(ContextRecord)->Esp;
      if (Esp >= dwStackLow && Esp <= dwStackHigh) {
        _newebp = Esp;
        StackFrame->Reserved[0] = 1;
        _newebp =
            SearchRecord(_newebp, dwStackHigh, hProcess, ReadMemoryRoutine);
      }
    }
    StackFrame->AddrFrame.Offset = _newebp;
  } else {
    const DWORD _ebp = StackFrame->AddrFrame.Offset;
    if (_ebp < dwStackLow || _ebp > dwStackHigh)
      return FALSE;
    StackFrame->AddrPC.Offset = StackFrame->AddrReturn.Offset;

    // 1 : 禅让模式。优先选择它。
    // 2 : 懒惰搜索模式。
    // 3 : 完全搜索模式 or 贪婪搜索模式
    if (nSearchMode == 1 || nSearchMode == 2) {
      //为2检测是否需要搜索。如果不需要搜索就直接用。
      //为1,只有在初始状态才检测是否需要搜索。
      if (nSearchMode == 1 && StackFrame->Reserved[0] == 1) {
        StackFrame->AddrFrame.Offset = 0;
      } else {
        DWORD _newebp;

        StackFrame->AddrFrame.Offset = 0;
        if (ReadMemoryRoutine(
                hProcess, _ebp, &_newebp, sizeof(_newebp), &readed) &&
            readed == sizeof(_newebp)) {
          if (_newebp > (DWORD)_ebp && _newebp > dwStackLow &&
              _newebp <= dwStackHigh) {
            StackFrame->AddrFrame.Offset = _newebp;
            DWORD raddr;
            if (ReadMemoryRoutine(
                    hProcess, (_newebp + 4), &raddr, sizeof(raddr), &readed) &&
                readed == sizeof(raddr)) {
              if ((raddr & 0xffff0000) == 0) {
                StackFrame->AddrFrame.Offset = 0;
              }
            }
          }
        }
      }
    } else if (nSearchMode == 3) {
      StackFrame->AddrFrame.Offset = 0;  // ALWAYS search for it.
    }

    if (StackFrame->AddrFrame.Offset == 0)  //是否启用搜索。 .
    {
      //标记开始搜索。
      StackFrame->Reserved[0] = 1;
      if (_ebp + 0x8 >= dwStackHigh) {
        StackFrame->AddrFrame.Offset = 0;
      } else {
        DWORD _newebp =
            SearchRecord(_ebp + 4, dwStackHigh, hProcess, ReadMemoryRoutine);
        StackFrame->AddrFrame.Offset = _newebp;
      }
    }
  }

  {
    DWORD _ebp = StackFrame->AddrFrame.Offset;
    if (_ebp != NULL) {
      DWORD vbuf[6] = {0};
      ReadMemoryRoutine(hProcess, _ebp, &vbuf, sizeof(vbuf), &readed);
      StackFrame->AddrReturn.Offset = vbuf[1];
      StackFrame->Params[0] = vbuf[2];
      StackFrame->Params[1] = vbuf[3];
      StackFrame->Params[2] = vbuf[4];
      StackFrame->Params[3] = vbuf[5];
    } else {
      StackFrame->AddrReturn.Offset = 0;
      StackFrame->Params[0] = 0;
      StackFrame->Params[1] = 0;
      StackFrame->Params[2] = 0;
      StackFrame->Params[3] = 0;
      return FALSE;
    }
  }
  GetModuleBaseRoutine(hProcess, StackFrame->AddrPC.Offset);
  return TRUE;
}
//////////////////////////////////////////////////////////////////////////

DWORD __stdcall GetModBase(HANDLE hProcess, DWORD dwAddr) {
  // This is what the MFC stack trace routines forgot to do so their
  //  code will not get the info out of the symbol engine.
  IMAGEHLP_MODULE stIHM = {sizeof(stIHM)};
  DWORD dwRet = 0;

  if (SymGetModuleInfo(hProcess, dwAddr, &stIHM)) {
    dwRet = stIHM.BaseOfImage;
  } else {
    // Let's go fishing.
    MEMORY_BASIC_INFORMATION stMBI;

    if (0 != VirtualQueryEx(hProcess, (LPCVOID)dwAddr, &stMBI, sizeof(stMBI))) {
      DWORD dwNameLen = 0;
      CHAR szFile[MAX_PATH] = {0};
      HANDLE hFile = NULL;

      dwNameLen = GetModuleFileNameExA(
          hProcess, (HINSTANCE)stMBI.AllocationBase, szFile, MAX_PATH);
      if (0 != dwNameLen) {
        hFile = CreateFileA(
            szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0);
        SymLoadModule(hProcess,
                      hFile,
                      (dwNameLen ? szFile : NULL),
                      NULL,
                      (DWORD)stMBI.AllocationBase,
                      0);
        CloseHandle(hFile);
      }
      dwRet = (DWORD)stMBI.AllocationBase;
    }
  }
  return dwRet;
}

static void __stdcall ConvertAddressA(HANDLE hProcess,
                                      LPSTR lpszAddresses,
                                      DWORD dwFrame,
                                      DWORD dwAddr,
                                      DWORD dwParm1,
                                      DWORD dwParm2,
                                      DWORD dwParm3,
                                      DWORD dwParm4) {
#define STACK_LEN 4000
  CHAR szTempBuf[STACK_LEN + sizeof(IMAGEHLP_SYMBOL)] = {0};
  PIMAGEHLP_SYMBOL pIHS = (PIMAGEHLP_SYMBOL)&szTempBuf;
  IMAGEHLP_MODULE stIHM = {sizeof(IMAGEHLP_MODULE)};

  pIHS->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
  pIHS->Address = dwAddr;
  pIHS->MaxNameLength = STACK_LEN;

  GetModBase(hProcess, dwAddr);
  if (SymGetModuleInfo(hProcess, dwAddr, &stIHM)) {
    LPSTR szName = strrchr(stIHM.ImageName, '\\');
    if (szName)
      ++szName;
    else
      szName = stIHM.ImageName;
    _snprintf(lpszAddresses,
              MAX_PATH - 1,
              "%s::%08x;",
              szName,
              dwAddr - stIHM.BaseOfImage);
  } else
    lpszAddresses[0] = '\0';
  lpszAddresses += strlen(lpszAddresses);
}

void __stdcall FilterModuleName(std::string& strTempStack) {
  if (strTempStack.length() < 11)
    return;
  std::string strTempModuleName =
      strTempStack.substr(0, strTempStack.length() - 11);
  strTempStack = strTempModuleName + ";";
}

void GetCallStackForBugReport(HANDLE hProcess,
                               HANDLE hThread,
                               const CONTEXT& stCtx,
                               __out std::string& strStackForHash) {
  PSYMBOL_INFO pSymbolInfo = (PSYMBOL_INFO)LocalAlloc(
      LPTR,
      sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR) + sizeof(ULONG64) - 1);
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_EXACT_SYMBOLS |
                SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_IGNORE_CVREC |
                SYMOPT_LOAD_LINES);
  SymInitialize(hProcess, NULL, FALSE);

  STACKFRAME stFrame;
  memset((void*)&stFrame, 0, sizeof(STACKFRAME));
  stFrame.AddrPC.Offset = stCtx.Eip;
  stFrame.AddrPC.Mode = AddrModeFlat;
  stFrame.AddrFrame.Offset = stCtx.Ebp;
  stFrame.AddrFrame.Mode = AddrModeFlat;

  int nLevel = 50;
  int nStackHashCount = 0;
  CONTEXT ctxCopy(stCtx);
  int nSearchMode = 1;
  for (int i = 0; i < nLevel; i++) {
    if (MyStackWalk(IMAGE_FILE_MACHINE_I386,
                    hProcess,
                    hThread,
                    &stFrame,
                    &ctxCopy,
                    (PREAD_PROCESS_MEMORY_ROUTINE)&ReadProcessMemory,
                    SymFunctionTableAccess,
                    GetModBase,
                    NULL,
                    nSearchMode)) {
      // Also check that the address is not zero.  Sometimes StackWalk returns
      // TRUE with a frame of zero.
      if (stFrame.AddrPC.Offset) {
        CHAR szForHash[MAX_PATH];
        memset(szForHash, 0, MAX_PATH);

        ConvertAddressA(hProcess,
                        szForHash,
                        stFrame.AddrFrame.Offset,
                        stFrame.AddrPC.Offset,
                        stFrame.Params[0],
                        stFrame.Params[1],
                        stFrame.Params[2],
                        stFrame.Params[3]);

        // strStackForHash最多取前12个调用....
        if (nStackHashCount <= 12) {
          std::string strTempStack = szForHash;
          std::transform(strTempStack.begin(),
                         strTempStack.end(),
                         strTempStack.begin(),
                         ::tolower);
          //FilterModuleName(strTempStack);
          strStackForHash += strTempStack;
        }
        nStackHashCount++;
      }
    } else {
      break;
    }
  }
  if (pSymbolInfo) {
    LocalFree(pSymbolInfo);
  }
  SymCleanup(hProcess);
}


std::string GetFileVersion(const std::string& file_path) {
  std::string version;
  if (file_path.empty()) {
    return version;
  }
  DWORD dw_handle;
  DWORD dw_len = ::GetFileVersionInfoSizeA(file_path.c_str(), &dw_handle);
  BYTE* lp_data = new BYTE[dw_len];
  if (!::GetFileVersionInfoA(file_path.c_str(), dw_handle, dw_len, (LPVOID)lp_data)) {
    return version;
  }
  VS_FIXEDFILEINFO* lp_buffer = NULL;
  UINT len = 0;
  if (!VerQueryValue(lp_data, L"\\", (LPVOID*)&lp_buffer, &len)) {
    return version;
  }
  version = std::to_string((lp_buffer->dwFileVersionMS >> 16) & 0xffff) + "." +
            std::to_string((lp_buffer->dwFileVersionMS & 0xffff)) + "." +
            std::to_string((lp_buffer->dwFileVersionLS >> 16) & 0xffff) + "." +
            std::to_string((lp_buffer->dwFileVersionLS & 0xffff));
  delete[] lp_data;
  return version;
}

void GetConfigInfo(std::string& version,
                   std::string& pn,
                   std::string& tn) {
  char buffer[MAX_PATH];
  ::GetModuleFileNameA(NULL, buffer, MAX_PATH);
  std::string file_path = buffer;
  version = GetFileVersion(file_path);
  std::string config_file_path;
  if (file_path.find_last_of('/') != std::string::npos) {
    config_file_path = file_path.substr(0, file_path.find_last_of('/'));
  } else if (file_path.find_last_of('\\') != std::string::npos) {
    config_file_path = file_path.substr(0, file_path.find_last_of('\\'));
  }
  config_file_path += "/app.json";
  if (PathFileExistsA(config_file_path.c_str())) {
    std::ifstream read_json(config_file_path);
    std::string config_context =
    std::string((std::istreambuf_iterator<char>(read_json)),
                    std::istreambuf_iterator<char>());
    CefRefPtr<CefValue> json_object =
        CefParseJSON(config_context, JSON_PARSER_RFC);
    if (json_object->IsValid()) {
      CefRefPtr<CefDictionaryValue> dict = json_object->GetDictionary();
      if (dict->IsValid()) {
        pn = dict->GetString("pn");
        tn = dict->GetString("tn");
      }
    }
  }
  return;
}
// wangbo end

CrashReportExceptionHandler::CrashReportExceptionHandler(
    CrashReportDatabase* database,
    CrashReportUploadThread* upload_thread,
    const std::map<std::string, std::string>* process_annotations,
    const UserStreamDataSources* user_stream_data_sources)
    : database_(database),
      upload_thread_(upload_thread),
      process_annotations_(process_annotations),
      user_stream_data_sources_(user_stream_data_sources) {}

CrashReportExceptionHandler::~CrashReportExceptionHandler() {
}

void CrashReportExceptionHandler::ExceptionHandlerServerStarted() {
}

unsigned int CrashReportExceptionHandler::ExceptionHandlerServerException(
    HANDLE process,
    WinVMAddress exception_information_address,
    WinVMAddress debug_critical_section_address) {
  Metrics::ExceptionEncountered();

  ScopedProcessSuspend suspend(process);

  ProcessSnapshotWin process_snapshot;
  if (!process_snapshot.Initialize(process,
                                   ProcessSuspensionState::kSuspended,
                                   exception_information_address,
                                   debug_critical_section_address)) {
    Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSnapshotFailed);
    return kTerminationCodeSnapshotFailed;
  }

  // Now that we have the exception information, even if something else fails we
  // can terminate the process with the correct exit code.
  const unsigned int termination_code =
      process_snapshot.Exception()->Exception();
  static_assert(
      std::is_same<std::remove_const<decltype(termination_code)>::type,
                   decltype(process_snapshot.Exception()->Exception())>::value,
      "expected ExceptionCode() and process termination code to match");

  Metrics::ExceptionCode(termination_code);

  CrashpadInfoClientOptions client_options;
  process_snapshot.GetCrashpadOptions(&client_options);
  if (client_options.crashpad_handler_behavior != TriState::kDisabled) {
    UUID client_id;
    Settings* const settings = database_->GetSettings();
    if (settings) {
      // If GetSettings() or GetClientID() fails, something else will log a
      // message and client_id will be left at its default value, all zeroes,
      // which is appropriate.
      settings->GetClientID(&client_id);
    }

    process_snapshot.SetClientID(client_id);
    process_snapshot.SetAnnotationsSimpleMap(*process_annotations_);

    std::unique_ptr<CrashReportDatabase::NewReport> new_report;
    CrashReportDatabase::OperationStatus database_status =
        database_->PrepareNewCrashReport(&new_report);
    if (database_status != CrashReportDatabase::kNoError) {
      LOG(ERROR) << "PrepareNewCrashReport failed";
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kPrepareNewCrashReportFailed);
      return termination_code;
    }

    process_snapshot.SetReportID(new_report->ReportID());

    // wangbo start crashpad unique hash
    DUMP_RECORD dump_record = {0};      
    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(&process_snapshot, dump_record);
    // wangbo end
    AddUserExtensionStreams(
        user_stream_data_sources_, &process_snapshot, &minidump);

    if (!minidump.WriteEverything(new_report->Writer())) {
      LOG(ERROR) << "WriteEverything failed";
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kMinidumpWriteFailed);
      return termination_code;
    }

    // wangbo start crashpad unique hash
    {
      CONTEXT context = process_snapshot.ContextRaw();
      HANDLE thread = OpenThread(
          THREAD_QUERY_INFORMATION, FALSE, process_snapshot.ThreadID());
      std::string stack_content;
      GetCallStackForBugReport(process, thread, context, stack_content);
      CloseHandle(thread);

      std::string version, pn, tn;
      GetConfigInfo(version, pn, tn);

      std::wstring txt_path = new_report->FileRemover().get().value();
      size_t pos = txt_path.find(L".dmp");
      if (pos != std::string::npos) {
        txt_path = txt_path.substr(0, pos);
      }
      txt_path += L".txt";

      std::string process_type = "Process Type: ";
      process_type += process_snapshot.ProcessType();

      std::string signature = "Crash Signature: ";
      std::string signature_tmp =
          "eip=" + std::to_string(dump_record.ContextRecordEip & 0xffff) +
          "&code=" + std::to_string(dump_record.ExceptionCode) +
          "&stack=" + stack_content;
      signature += base::MD5String(signature_tmp);

      std::ofstream ofs(txt_path.c_str());
      ofs << process_type;
      ofs << "\r\n";
      ofs << signature;
      if (!version.empty()) {
        ofs << "\r\n";
        ofs << "Version: ";
        ofs << version;
      }
      if (!pn.empty()) {
        ofs << "\r\n";
        ofs << "Pn: ";
        ofs << pn;
      }
      if (!tn.empty()) {
        ofs << "\r\n";
        ofs << "Tn: ";
        ofs << tn;
      }
      ofs.close();
    }
    // wangbo end

    UUID uuid;
    database_status =
        database_->FinishedWritingCrashReport(std::move(new_report), &uuid);
    if (database_status != CrashReportDatabase::kNoError) {
      LOG(ERROR) << "FinishedWritingCrashReport failed";
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
      return termination_code;
    }

    if (upload_thread_) {
      upload_thread_->ReportPending(uuid);
    }
  }

  Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSuccess);
  return termination_code;
}

}  // namespace crashpad
