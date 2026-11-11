// SNKHD - funzioni OpenGL 3.3 caricate con SDL_GL_GetProcAddress
#ifndef SNKHD_GLFN_H
#define SNKHD_GLFN_H
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif
typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean, GLubyte;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr, GLintptr;

#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_BGRA 0x80E1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 0x0004
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_TEXTURE0 0x84C0
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_RED 0x1903
#define GL_R8 0x8229
#define GL_SCISSOR_TEST 0x0C11
#define GL_TEXTURE_SWIZZLE_RGBA 0x8E46
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02

#define GLFN_LIST \
 X(void, GenTextures, (GLsizei, GLuint*)) \
 X(void, DeleteTextures, (GLsizei, const GLuint*)) \
 X(void, BindTexture, (GLenum, GLuint)) \
 X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
 X(void, TexSubImage2D, (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)) \
 X(void, TexParameteri, (GLenum, GLenum, GLint)) \
 X(void, TexParameteriv, (GLenum, GLenum, const GLint*)) \
 X(void, GenerateMipmap, (GLenum)) \
 X(void, PixelStorei, (GLenum, GLint)) \
 X(void, ActiveTexture, (GLenum)) \
 X(void, Viewport, (GLint, GLint, GLsizei, GLsizei)) \
 X(void, Scissor, (GLint, GLint, GLsizei, GLsizei)) \
 X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat)) \
 X(void, Clear, (GLbitfield)) \
 X(void, Enable, (GLenum)) \
 X(void, Disable, (GLenum)) \
 X(void, BlendFunc, (GLenum, GLenum)) \
 X(GLuint, CreateShader, (GLenum)) \
 X(void, ShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
 X(void, CompileShader, (GLuint)) \
 X(void, GetShaderiv, (GLuint, GLenum, GLint*)) \
 X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
 X(GLuint, CreateProgram, (void)) \
 X(void, AttachShader, (GLuint, GLuint)) \
 X(void, LinkProgram, (GLuint)) \
 X(void, GetProgramiv, (GLuint, GLenum, GLint*)) \
 X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*)) \
 X(void, UseProgram, (GLuint)) \
 X(void, BindAttribLocation, (GLuint, GLuint, const GLchar*)) \
 X(GLint, GetUniformLocation, (GLuint, const GLchar*)) \
 X(void, Uniform1i, (GLint, GLint)) \
 X(void, Uniform1f, (GLint, GLfloat)) \
 X(void, Uniform2f, (GLint, GLfloat, GLfloat)) \
 X(void, Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
 X(void, GenVertexArrays, (GLsizei, GLuint*)) \
 X(void, BindVertexArray, (GLuint)) \
 X(void, GenBuffers, (GLsizei, GLuint*)) \
 X(void, BindBuffer, (GLenum, GLuint)) \
 X(void, BufferData, (GLenum, GLsizeiptr, const void*, GLenum)) \
 X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
 X(void, EnableVertexAttribArray, (GLuint)) \
 X(void, DrawArrays, (GLenum, GLint, GLsizei)) \
 X(void, GenFramebuffers, (GLsizei, GLuint*)) \
 X(void, DeleteFramebuffers, (GLsizei, const GLuint*)) \
 X(void, BindFramebuffer, (GLenum, GLuint)) \
 X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint)) \
 X(void, Finish, (void)) \
 X(const GLubyte*, GetString, (GLenum))

#define X(ret, name, args) typedef ret (GLAPIENTRY *PFN_gl##name) args; extern PFN_gl##name gl##name;
GLFN_LIST
#undef X

int glfn_load(void *(*getproc)(const char*));
#endif
