#ifndef __GL_H_SHIM__
#define __GL_H_SHIM__

#include "glew.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- GLEW shim entry points ---- */
GLenum glewInit(void);
GLboolean glewIsSupported(const char* name);

/* GLEW function-pointer globals referenced by the game */
extern void (*__glewDepthRangef)(GLclampf zNear, GLclampf zFar);
extern void (*__glewBindBuffer)(GLenum target, GLuint buffer);

/* ---- Core GL 1.x surface used by the game ---- */

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void glClearDepthf(GLclampf depth);
void glClearDepth(GLclampd depth);
void glClear(GLbitfield mask);

void glEnable(GLenum cap);
void glDisable(GLenum cap);
GLboolean glIsEnabled(GLenum cap);
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
void glDepthMask(GLboolean flag);
void glDepthFunc(GLenum func);
void glDepthRange(GLclampd zNear, GLclampd zFar);
void glDepthRangef(GLclampf zNear, GLclampf zFar);
void glBlendFunc(GLenum sfactor, GLenum dfactor);
void glAlphaFunc(GLenum func, GLclampf ref);
void glShadeModel(GLenum mode);
void glCullFace(GLenum mode);
void glLineWidth(GLfloat width);
void glPolygonOffset(GLfloat factor, GLfloat units);
void glPolygonMode(GLenum face, GLenum mode);
void glHint(GLenum target, GLenum mode);
void glColorMaterial(GLenum face, GLenum mode);

void glFogf(GLenum pname, GLfloat param);
void glFogi(GLenum pname, GLint param);
void glFogfv(GLenum pname, const GLfloat* params);

void glMatrixMode(GLenum mode);
void glLoadIdentity(void);
void glLoadMatrixf(const GLfloat* m);
void glMultMatrixf(const GLfloat* m);
void glPushMatrix(void);
void glPopMatrix(void);
void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);

void glColor3f(GLfloat r, GLfloat g, GLfloat b);
void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz);

void glGenTextures(GLsizei n, GLuint* textures);
void glDeleteTextures(GLsizei n, const GLuint* textures);
void glBindTexture(GLenum target, GLuint texture);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* pixels);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* pixels);
void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data);
void glPixelStorei(GLenum pname, GLint param);

void glBindBuffer(GLenum target, GLuint buffer);
void glGenBuffers(GLsizei n, GLuint* buffers);
void glDeleteBuffers(GLsizei n, const GLuint* buffers);
void glBufferData(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage);
void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data);

void glEnableClientState(GLenum array);
void glDisableClientState(GLenum array);
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void glNormalPointer(GLenum type, GLsizei stride, const GLvoid* pointer);

void glDrawArrays(GLenum mode, GLint first, GLsizei count);
void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

GLuint glGenLists(GLsizei range);
void glDeleteLists(GLuint list, GLsizei range);
void glNewList(GLuint list, GLenum mode);
void glEndList(void);
void glCallList(GLuint list);
void glCallLists(GLsizei n, GLenum type, const GLvoid* lists);

GLenum glGetError(void);
const GLubyte* glGetString(GLenum name);
void glGetFloatv(GLenum pname, GLfloat* params);
void glGetIntegerv(GLenum pname, GLint* params);
void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels);
void glFinish(void);
void glFlush(void);

/* wgl functions are declared by windows.h/wingdi.h, not by this shim.
   No declarations here to avoid calling-convention conflicts. */

#ifdef __cplusplus
}
#endif

#endif /* __GL_H_SHIM__ */