// =============================================================================
// GLAD Implementation - OpenGL 4.5 Core Loader
// =============================================================================

#include <glad/glad.h>

// Function pointer definitions
PFNGLCLEARPROC glClear = 0;
PFNGLCLEARCOLORPROC glClearColor = 0;
PFNGLVIEWPORTPROC glViewport = 0;
PFNGLENABLEPROC glEnable = 0;
PFNGLDISABLEPROC glDisable = 0;
PFNGLCULLFACEPROC glCullFace = 0;
PFNGLFRONTFACEPROC glFrontFace = 0;
PFNGLPOLYGONMODEPROC glPolygonMode = 0;
PFNGLGETSTRINGPROC glGetString = 0;
PFNGLGETINTEGERVPROC glGetIntegerv = 0;
PFNGLBLENDFUNCPROC glBlendFunc = 0;

PFNGLCREATEBUFFERSPROC glCreateBuffers = 0;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = 0;
PFNGLBINDBUFFERPROC glBindBuffer = 0;
PFNGLNAMEDBUFFERSTORAGEPROC glNamedBufferStorage = 0;
PFNGLNAMEDBUFFERDATAPROC glNamedBufferData = 0;
PFNGLNAMEDBUFFERSUBDATAPROC glNamedBufferSubData = 0;
PFNGLMAPNAMEDBUFFERRANGEPROC glMapNamedBufferRange = 0;
PFNGLUNMAPNAMEDBUFFERPROC glUnmapNamedBuffer = 0;

PFNGLCREATEVERTEXARRAYSPROC glCreateVertexArrays = 0;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = 0;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = 0;
PFNGLENABLEVERTEXARRAYATTRIBPROC glEnableVertexArrayAttrib = 0;
PFNGLVERTEXARRAYATTRIBFORMATPROC glVertexArrayAttribFormat = 0;
PFNGLVERTEXARRAYATTRIBIFORMATPROC glVertexArrayAttribIFormat = 0;
PFNGLVERTEXARRAYATTRIBBINDINGPROC glVertexArrayAttribBinding = 0;
PFNGLVERTEXARRAYVERTEXBUFFERPROC glVertexArrayVertexBuffer = 0;
PFNGLVERTEXARRAYELEMENTBUFFERPROC glVertexArrayElementBuffer = 0;

PFNGLCREATESHADERPROC glCreateShader = 0;
PFNGLDELETESHADERPROC glDeleteShader = 0;
PFNGLSHADERSOURCEPROC glShaderSource = 0;
PFNGLCOMPILESHADERPROC glCompileShader = 0;
PFNGLGETSHADERIVPROC glGetShaderiv = 0;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = 0;

PFNGLCREATEPROGRAMPROC glCreateProgram = 0;
PFNGLDELETEPROGRAMPROC glDeleteProgram = 0;
PFNGLATTACHSHADERPROC glAttachShader = 0;
PFNGLDETACHSHADERPROC glDetachShader = 0;
PFNGLLINKPROGRAMPROC glLinkProgram = 0;
PFNGLUSEPROGRAMPROC glUseProgram = 0;
PFNGLGETPROGRAMIVPROC glGetProgramiv = 0;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = 0;

PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = 0;
PFNGLUNIFORM1IPROC glUniform1i = 0;
PFNGLUNIFORM1UIPROC glUniform1ui = 0;
PFNGLUNIFORM1FPROC glUniform1f = 0;
PFNGLUNIFORM2FPROC glUniform2f = 0;
PFNGLUNIFORM3FPROC glUniform3f = 0;
PFNGLUNIFORM4FPROC glUniform4f = 0;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = 0;

PFNGLDRAWARRAYSPROC glDrawArrays = 0;
PFNGLDRAWELEMENTSPROC glDrawElements = 0;
PFNGLMULTIDRAWELEMENTSINDIRECTPROC glMultiDrawElementsIndirect = 0;

PFNGLFENCESYNCPROC glFenceSync = 0;
PFNGLDELETESYNCPROC glDeleteSync = 0;
PFNGLCLIENTWAITSYNCPROC glClientWaitSync = 0;

PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback = 0;

static void* get_proc(GLADloadproc load, const char* name) {
    void* result = load(name);
    return result;
}

int gladLoadGLLoader(GLADloadproc load) {
    glClear = (PFNGLCLEARPROC)get_proc(load, "glClear");
    glClearColor = (PFNGLCLEARCOLORPROC)get_proc(load, "glClearColor");
    glViewport = (PFNGLVIEWPORTPROC)get_proc(load, "glViewport");
    glEnable = (PFNGLENABLEPROC)get_proc(load, "glEnable");
    glDisable = (PFNGLDISABLEPROC)get_proc(load, "glDisable");
    glCullFace = (PFNGLCULLFACEPROC)get_proc(load, "glCullFace");
    glFrontFace = (PFNGLFRONTFACEPROC)get_proc(load, "glFrontFace");
    glPolygonMode = (PFNGLPOLYGONMODEPROC)get_proc(load, "glPolygonMode");
    glGetString = (PFNGLGETSTRINGPROC)get_proc(load, "glGetString");
    glGetIntegerv = (PFNGLGETINTEGERVPROC)get_proc(load, "glGetIntegerv");
    glBlendFunc = (PFNGLBLENDFUNCPROC)get_proc(load, "glBlendFunc");

    glCreateBuffers = (PFNGLCREATEBUFFERSPROC)get_proc(load, "glCreateBuffers");
    glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)get_proc(load, "glDeleteBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)get_proc(load, "glBindBuffer");
    glNamedBufferStorage = (PFNGLNAMEDBUFFERSTORAGEPROC)get_proc(load, "glNamedBufferStorage");
    glNamedBufferData = (PFNGLNAMEDBUFFERDATAPROC)get_proc(load, "glNamedBufferData");
    glNamedBufferSubData = (PFNGLNAMEDBUFFERSUBDATAPROC)get_proc(load, "glNamedBufferSubData");
    glMapNamedBufferRange = (PFNGLMAPNAMEDBUFFERRANGEPROC)get_proc(load, "glMapNamedBufferRange");
    glUnmapNamedBuffer = (PFNGLUNMAPNAMEDBUFFERPROC)get_proc(load, "glUnmapNamedBuffer");

    glCreateVertexArrays = (PFNGLCREATEVERTEXARRAYSPROC)get_proc(load, "glCreateVertexArrays");
    glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)get_proc(load, "glDeleteVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)get_proc(load, "glBindVertexArray");
    glEnableVertexArrayAttrib = (PFNGLENABLEVERTEXARRAYATTRIBPROC)get_proc(load, "glEnableVertexArrayAttrib");
    glVertexArrayAttribFormat = (PFNGLVERTEXARRAYATTRIBFORMATPROC)get_proc(load, "glVertexArrayAttribFormat");
    glVertexArrayAttribIFormat = (PFNGLVERTEXARRAYATTRIBIFORMATPROC)get_proc(load, "glVertexArrayAttribIFormat");
    glVertexArrayAttribBinding = (PFNGLVERTEXARRAYATTRIBBINDINGPROC)get_proc(load, "glVertexArrayAttribBinding");
    glVertexArrayVertexBuffer = (PFNGLVERTEXARRAYVERTEXBUFFERPROC)get_proc(load, "glVertexArrayVertexBuffer");
    glVertexArrayElementBuffer = (PFNGLVERTEXARRAYELEMENTBUFFERPROC)get_proc(load, "glVertexArrayElementBuffer");

    glCreateShader = (PFNGLCREATESHADERPROC)get_proc(load, "glCreateShader");
    glDeleteShader = (PFNGLDELETESHADERPROC)get_proc(load, "glDeleteShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)get_proc(load, "glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)get_proc(load, "glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)get_proc(load, "glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)get_proc(load, "glGetShaderInfoLog");

    glCreateProgram = (PFNGLCREATEPROGRAMPROC)get_proc(load, "glCreateProgram");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)get_proc(load, "glDeleteProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)get_proc(load, "glAttachShader");
    glDetachShader = (PFNGLDETACHSHADERPROC)get_proc(load, "glDetachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)get_proc(load, "glLinkProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)get_proc(load, "glUseProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)get_proc(load, "glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)get_proc(load, "glGetProgramInfoLog");

    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)get_proc(load, "glGetUniformLocation");
    glUniform1i = (PFNGLUNIFORM1IPROC)get_proc(load, "glUniform1i");
    glUniform1ui = (PFNGLUNIFORM1UIPROC)get_proc(load, "glUniform1ui");
    glUniform1f = (PFNGLUNIFORM1FPROC)get_proc(load, "glUniform1f");
    glUniform2f = (PFNGLUNIFORM2FPROC)get_proc(load, "glUniform2f");
    glUniform3f = (PFNGLUNIFORM3FPROC)get_proc(load, "glUniform3f");
    glUniform4f = (PFNGLUNIFORM4FPROC)get_proc(load, "glUniform4f");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)get_proc(load, "glUniformMatrix4fv");

    glDrawArrays = (PFNGLDRAWARRAYSPROC)get_proc(load, "glDrawArrays");
    glDrawElements = (PFNGLDRAWELEMENTSPROC)get_proc(load, "glDrawElements");
    glMultiDrawElementsIndirect = (PFNGLMULTIDRAWELEMENTSINDIRECTPROC)get_proc(load, "glMultiDrawElementsIndirect");

    glFenceSync = (PFNGLFENCESYNCPROC)get_proc(load, "glFenceSync");
    glDeleteSync = (PFNGLDELETESYNCPROC)get_proc(load, "glDeleteSync");
    glClientWaitSync = (PFNGLCLIENTWAITSYNCPROC)get_proc(load, "glClientWaitSync");

    glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)get_proc(load, "glDebugMessageCallback");

    // Check required functions
    if (!glClear || !glClearColor || !glCreateBuffers || !glCreateVertexArrays ||
        !glCreateShader || !glCreateProgram || !glDrawElements) {
        return 0;
    }

    return 1;
}
