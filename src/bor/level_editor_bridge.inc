/* External definition editor bridge. GPL-3.0-or-later. Signed: Atlas. */
static char bsl_editor_status[160] = "Shared definitions open externally; instance edits stay here.";
#ifdef _WIN32
static wchar_t bsl_editor_request[32768], bsl_editor_reply[32768];
static char bsl_editor_token[33];
static ULONGLONG bsl_editor_started;
static unsigned bsl_editor_serial;
static bool bsl_editor_cleanup_registered;
static void bsl_editor_clean(void)
{
    if(*bsl_editor_request) DeleteFileW(bsl_editor_request);
    if(*bsl_editor_reply) DeleteFileW(bsl_editor_reply);
    *bsl_editor_request = *bsl_editor_reply = 0;
}
static bool bsl_editor_arg(wchar_t* output, size_t capacity, const wchar_t* arg)
{
    size_t n = wcslen(output), slashes = 0;
    /* Windows CommandLineToArgvW quoting, including trailing backslashes. */
#define BSL_APPEND(c) do { if(n + 2 >= capacity) return false; output[n++] = (c); output[n] = 0; } while(0)
    if(n) { BSL_APPEND(L' '); }
    BSL_APPEND(L'"');
    for(const wchar_t* p = arg; ; p++) {
        if(*p == L'\\') { slashes++; continue; }
        if(*p == L'"' || !*p) {
            for(size_t i = 0; i < slashes * 2; i++) BSL_APPEND(L'\\');
            if(*p == L'"') BSL_APPEND(L'\\');
        }
        else for(size_t i = 0; i < slashes; i++) BSL_APPEND(L'\\');
        slashes = 0;
        if(!*p) break;
        BSL_APPEND(*p);
    }
    BSL_APPEND(L'"');
#undef BSL_APPEND
    return true;
}
static bool bsl_editor_json(char* output, size_t capacity, size_t* n, const char* value)
{
#define BSL_JSON(c) do { if(*n + 2 >= capacity) return false; output[(*n)++] = (c); output[*n] = 0; } while(0)
    BSL_JSON('"');
    for(const unsigned char* p = (const unsigned char*)value; *p; p++) {
        if(*p < 32) return false;
        if(*p == '"' || *p == '\\') BSL_JSON('\\');
        BSL_JSON(*p);
    }
    BSL_JSON('"');
#undef BSL_JSON
    return true;
}
static void bsl_editor_poll(void)
{
    if(!*bsl_editor_request) return;
    HANDLE handle = CreateFileW(bsl_editor_reply, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(handle != INVALID_HANDLE_VALUE) {
        char bytes[192] = {0}, token[33] = {0}, status[32] = {0}, extra; DWORD count = 0;
        bool ok = ReadFile(handle, bytes, sizeof bytes - 1, &count, NULL); CloseHandle(handle);
        if(ok && sscanf(bytes, "BSL_EDITOR_REPLY_V1 %32s %31s %c", token, status, &extra) == 2 && !strcmp(token, bsl_editor_token)) {
            snprintf(bsl_editor_status, sizeof bsl_editor_status, "RPG editor: %s%s", status, !strcmp(status, "opened") ? " (existing draft preserved)" : "");
            bsl_editor_clean(); return;
        }
        str_cpy(bsl_editor_status, "RPG editor returned an invalid acknowledgment.", sizeof bsl_editor_status); bsl_editor_clean(); return;
    }
    if(GetTickCount64() - bsl_editor_started > 90000) {
        str_cpy(bsl_editor_status, "Editor acknowledgment timed out; check editor activity.", sizeof bsl_editor_status); bsl_editor_clean();
    }
}
static void bsl_editor_launch(surgescript_object_t* object, const char* definition)
{
    bsl_editor_poll();
    if(!bsl_editor_cleanup_registered) { atexit(bsl_editor_clean); bsl_editor_cleanup_registered = true; }
    if(*bsl_editor_request) { str_cpy(bsl_editor_status, "An editor request is still pending.", sizeof bsl_editor_status); return; }
    wchar_t executable[32768] = {0}, command[32768] = {0}, temp[32768] = {0};
    DWORD n = GetEnvironmentVariableW(L"BSL_RPG_EDITOR", executable, 32768);
    if(!n) {
        n = GetModuleFileNameW(NULL, executable, 32768);
        if(!n || n >= 32730) goto failure;
        wchar_t* slash = wcsrchr(executable, L'\\'); if(!slash) goto failure;
        slash[1] = 0; wcscat(executable, L"editor\\Editor.exe");
    }
    else if(n >= 32768) goto failure;
    if(GetFileAttributesW(executable) == INVALID_FILE_ATTRIBUTES) {
        str_cpy(bsl_editor_status, "RPG editor missing: expected editor/Editor.exe.", sizeof bsl_editor_status); return;
    }
    n = GetTempPathW(32768, temp); if(!n || n >= 32680) goto failure;
    snprintf(bsl_editor_token, sizeof bsl_editor_token, "%08lx%016llx%08x", (unsigned long)GetCurrentProcessId(), (unsigned long long)GetTickCount64(), ++bsl_editor_serial);
    wchar_t token[33]; for(int i = 0; i < 33; i++) token[i] = bsl_editor_token[i];
    swprintf(bsl_editor_request, 32768, L"%lsbsl-editor-%ls.json", temp, token);
    swprintf(bsl_editor_reply, 32768, L"%ls.reply", bsl_editor_request);
    bool teleporter = !strcmp(definition, "@teleporter");
    char request[8192]; size_t used = (size_t)snprintf(request, sizeof request, "{\"version\":1,\"token\":\"%s\",\"operation\":\"%s\",\"record_id\":", bsl_editor_token, teleporter ? "open_teleporter" : *definition ? "open_record" : "activate");
    if(!bsl_editor_json(request, sizeof request, &used, teleporter ? "" : definition)) goto failure;
#define BSL_LITERAL(s) do { const char* part = (s); size_t length = strlen(part); if(used + length + 1 >= sizeof request) goto failure; memcpy(request + used, part, length + 1); used += length; } while(0)
    BSL_LITERAL(",\"origin\":{\"level\":"); if(!bsl_editor_json(request, sizeof request, &used, file)) goto failure;
    BSL_LITERAL(",\"placement\":"); if(!bsl_editor_json(request, sizeof request, &used, object ? x64_to_str(entity_info_id(object), NULL, 0) : "")) goto failure;
    BSL_LITERAL(",\"class\":"); if(!bsl_editor_json(request, sizeof request, &used, object ? surgescript_object_name(object) : "")) goto failure;
    BSL_LITERAL("}}");
#undef BSL_LITERAL
    HANDLE handle = CreateFileW(bsl_editor_request, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if(handle == INVALID_HANDLE_VALUE) { *bsl_editor_request = *bsl_editor_reply = 0; goto failure; }
    DWORD wrote = 0; bool saved = WriteFile(handle, request, (DWORD)used, &wrote, NULL) && wrote == used; CloseHandle(handle); if(!saved) goto failure;
    if(!bsl_editor_arg(command, 32768, executable) || !bsl_editor_arg(command, 32768, L"--bsl-open-request") || !bsl_editor_arg(command, 32768, bsl_editor_request)) goto failure;
    const wchar_t* variables[] = {L"BSL_AUTHORING_PROJECT", L"BSL_AUTHORING_RECORDS", L"BSL_AUTHORING_PYTHON"};
    const wchar_t* options[] = {L"--bsl-project", L"--bsl-records", L"--bsl-python"};
    for(int i = 0; i < 3; i++) {
        wchar_t value[32768]; DWORD count = GetEnvironmentVariableW(variables[i], value, 32768);
        if(count >= 32768) goto failure;
        if(count && (!bsl_editor_arg(command, 32768, options[i]) || !bsl_editor_arg(command, 32768, value))) goto failure;
    }
    STARTUPINFOW startup = {0}; PROCESS_INFORMATION process = {0}; startup.cb = sizeof startup;
    if(!CreateProcessW(executable, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) goto failure;
    CloseHandle(process.hThread); CloseHandle(process.hProcess); bsl_editor_started = GetTickCount64();
    str_cpy(bsl_editor_status, "Opening RPG editor; waiting for acknowledgment...", sizeof bsl_editor_status); return;
failure:
    { DWORD error = GetLastError(); bsl_editor_clean(); snprintf(bsl_editor_status, sizeof bsl_editor_status, "Cannot open RPG editor (Windows error %lu).", (unsigned long)error); }
}
#else
static void bsl_editor_poll(void) {}
static void bsl_editor_launch(surgescript_object_t* object, const char* definition)
{
    (void)object; (void)definition; str_cpy(bsl_editor_status, "External editor launching currently supports Windows.", sizeof bsl_editor_status);
}
#endif
static bool bsl_editor_bridge_click(surgescript_object_t* object, int x, int y)
{
    if(y < 294 || y >= 315) return false;
    if(x >= 10 && x < 155) bsl_editor_launch(object, "");
    else if(x >= 165 && x < 320 && object && !strcmp(surgescript_object_name(object), "BSL Teleporter")) bsl_editor_launch(object, "@teleporter");
    else if(x >= 165 && x < 320 && bsl_event_is_anchor(object)) {
        const char* id = bsl_event_definition(entity_info_id(object));
        if(*id) bsl_editor_launch(object, id);
        else str_cpy(bsl_editor_status, "Bind a common event before opening its definition.", sizeof bsl_editor_status);
    }
    return true;
}
static void bsl_editor_bridge_render(surgescript_object_t* object, int x)
{
    image_rect(x + 10, 294, x + 154, 314, color_rgb(122, 156, 181)); bsl_inspect_text(x + 15, 298, "Open RPG editor");
    bool teleporter = object && !strcmp(surgescript_object_name(object), "BSL Teleporter");
    bool available = teleporter || (bsl_event_is_anchor(object) && *bsl_event_definition(entity_info_id(object)));
    image_rect(x + 165, 294, x + 319, 314, color_rgb(available ? 122 : 65, available ? 156 : 65, available ? 181 : 65));
    bool local = object && !strcmp(surgescript_object_name(object), "BSL Sequence Exit");
    bsl_inspect_text(x + 170, 298, local ? "Local helper" : teleporter ? "Edit teleporter" : "Edit shared event");
    char first[48]; snprintf(first, sizeof first, "%.46s", bsl_editor_status); bsl_inspect_text(x + 10, 322, first);
    if(strlen(bsl_editor_status) > 46) { snprintf(first, sizeof first, "%.46s", bsl_editor_status + 46); bsl_inspect_text(x + 10, 337, first); }
    bsl_inspect_text(x + 10, 358, "Ctrl+Z/Y undo/redo | Ctrl+S save level");
    bsl_inspect_text(x + 10, 376, local ? "Local helper; no DB binding required." : "No actor-definition binding yet.");
}
