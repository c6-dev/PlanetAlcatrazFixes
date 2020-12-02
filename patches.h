#pragma once
#define QPC_HOOK (void*)0x6CC1D4
#define QPC_HOOK_RU (void*)0x6761FC

bool ruExe = false;

__declspec(naked) void fixQuit() {
	static UInt32 retnAddr = 0x4288F0;
	static UInt32 jmpAddr = 0x4286D1;
	__asm {
		cmp dword ptr ss:[esp+0x28], 0x12
		jne CHECKHWND
		jmp RETURN
		CHECKHWND:
		cmp dword ptr ds : [0x010D81B0] , 0x0 // g_mainHwnd
		je RETURN
		jmp jmpAddr
		RETURN:
		jmp retnAddr
	}
}

__declspec(naked) void fixQuitRU() {
	static UInt32 retnAddr = 0x41E0B0;
	static UInt32 jmpAddr = 0x41DE91;
	__asm {
		cmp dword ptr ss : [esp + 0x28] , 0x12
		jne CHECKHWND
		jmp RETURN
		CHECKHWND :
		cmp dword ptr ds : [0xBFC2C0] , 0x0 // g_mainHwnd
			je RETURN
			jmp jmpAddr
			RETURN :
		jmp retnAddr
	}
}

void writePatches() {
	if (!ruExe) {
		WriteRelJump(0x4288E5, (UInt32)fixQuit); // EN 1.06
	}
	else  {
		WriteRelJump(0x41E0A5, (UInt32)fixQuitRU); // RU 1.05
	}

}

void* get_IAT_address(BYTE* base, const char* dll_name, const char* search)
{
	IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)base;
	IMAGE_NT_HEADERS* nt_headers = (IMAGE_NT_HEADERS*)(base + dos_header->e_lfanew);
	IMAGE_DATA_DIRECTORY section = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	IMAGE_IMPORT_DESCRIPTOR* import_descriptor = (IMAGE_IMPORT_DESCRIPTOR*)(base + section.VirtualAddress);
	for (size_t i = 0; import_descriptor[i].Name != NULL; i++)
	{
		if (!_stricmp((char*)(base + import_descriptor[i].Name), dll_name))
		{
			if (!import_descriptor[i].FirstThunk) { return nullptr; }
			IMAGE_THUNK_DATA* name_table = (IMAGE_THUNK_DATA*)(base + import_descriptor[i].OriginalFirstThunk);
			IMAGE_THUNK_DATA* import_table = (IMAGE_THUNK_DATA*)(base + import_descriptor[i].FirstThunk);
			for (; name_table->u1.Ordinal != NULL; ++name_table, ++import_table)
			{
				if (!IMAGE_SNAP_BY_ORDINAL(name_table->u1.Ordinal))
				{
					IMAGE_IMPORT_BY_NAME* import_name = (IMAGE_IMPORT_BY_NAME*)(base + name_table->u1.ForwarderString);
					char* func_name = &import_name->Name[0];
					if (!_stricmp(func_name, search)) { return &import_table->u1.AddressOfData; }
				}
			}
		}
	}
	return nullptr;
}


BOOL WINAPI qpc_hook(LARGE_INTEGER* lpPerformanceCount)
{
	writePatches();
	BYTE* base = (BYTE*)GetModuleHandle(NULL);
	void* address = get_IAT_address(base, "kernel32.dll", "QueryPerformanceCounter");
	// Restoring the original pointer
	SafeWrite32((UInt32)address, (UInt32)&QueryPerformanceCounter);
	Log() << "Patches installed, original QPC pointer restored.";
	return QueryPerformanceCounter(lpPerformanceCount);
}
// Hooking QueryPerformanceCounter (called after the exe is unpacked)
void createLoaderHook() {
	BYTE* base = (BYTE*)GetModuleHandle(NULL);
	void* address = get_IAT_address(base, "kernel32.dll", "QueryPerformanceCounter");
	if (address == QPC_HOOK || address == QPC_HOOK_RU)
	{
			if (address == QPC_HOOK_RU) ruExe = true;
			SafeWrite32((UInt32)address, (UInt32)&qpc_hook);
			Log() << "QPC hook installed.";
	}
}