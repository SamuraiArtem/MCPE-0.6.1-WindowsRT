#ifndef __GLEW_H_SHIM__
#define __GLEW_H_SHIM__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GL_TYPES_DEFINED
#define GL_TYPES_DEFINED
typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef void           GLvoid;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef float          GLfloat;
typedef float          GLclampf;
typedef double         GLdouble;
typedef double         GLclampd;
typedef long           GLintptr;
typedef long           GLsizeiptr;
typedef char           GLchar;
#endif

/* error codes */
#define GL_NO_ERROR          0x0000
#define GL_INVALID_ENUM      0x0500
#define GL_INVALID_VALUE     0x0501
#define GL_INVALID_OPERATION 0x0502
#define GL_STACK_OVERFLOW    0x0503
#define GL_STACK_UNDERFLOW   0x0504
#define GL_OUT_OF_MEMORY     0x0505

#define GL_FALSE 0
#define GL_TRUE  1

/* begin/end primitives */
#define GL_POINTS         0x0000
#define GL_LINES          0x0001
#define GL_LINE_LOOP      0x0002
#define GL_LINE_STRIP     0x0003
#define GL_TRIANGLES      0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN   0x0006
#define GL_QUADS          0x0007
#define GL_QUAD_STRIP     0x0008
#define GL_POLYGON        0x0009

/* blend factors */
#define GL_ZERO                    0
#define GL_ONE                     1
#define GL_SRC_COLOR               0x0300
#define GL_ONE_MINUS_SRC_COLOR     0x0301
#define GL_SRC_ALPHA               0x0302
#define GL_ONE_MINUS_SRC_ALPHA     0x0303
#define GL_DST_ALPHA               0x0304
#define GL_ONE_MINUS_DST_ALPHA     0x0305
#define GL_DST_COLOR               0x0306
#define GL_ONE_MINUS_DST_COLOR     0x0307
#define GL_SRC_ALPHA_SATURATE      0x0308

/* depth functions */
#define GL_NEVER    0x0200
#define GL_LESS     0x0201
#define GL_EQUAL    0x0202
#define GL_LEQUAL   0x0203
#define GL_GREATER  0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL   0x0206
#define GL_ALWAYS   0x0207

/* fog */
#define GL_EXP   0x0800
#define GL_EXP2  0x0801

/* faces */
#define GL_FRONT           0x0404
#define GL_BACK            0x0405
#define GL_FRONT_AND_BACK  0x0408
#define GL_CW              0x0900
#define GL_CCW             0x0901
#define GL_CULL_FACE       0x0B44
#define GL_CULL_FACE_MODE  0x0B45
#define GL_FRONT_FACE      0x0B46

/* pixel store */
#define GL_PACK_ALIGNMENT   0x0D05
#define GL_UNPACK_ALIGNMENT 0x0CF5

/* blender/offsets */
#define GL_STENCIL_TEST          0x0B90
#define GL_BLEND                 0x0BE2
#define GL_BLEND_SRC             0x0BE1
#define GL_BLEND_DST             0x0BE0
#define GL_SCISSOR_TEST          0x0C11
#define GL_ALPHA_TEST            0x0BC0
#define GL_ALPHA_TEST_FUNC       0x0BC1
#define GL_ALPHA_TEST_REF        0x0BC2
#define GL_LIGHTING              0x0B50
#define GL_NORMALIZE             0x0BA1
#define GL_COLOR_MATERIAL        0x0B57
#define GL_COLOR_MATERIAL_FACE   0x0B55
#define GL_COLOR_MATERIAL_PARAMETER 0x0B56
#define GL_FOG                   0x0B60
#define GL_FOG_INDEX             0x0B61
#define GL_FOG_DENSITY           0x0B62
#define GL_FOG_START             0x0B63
#define GL_FOG_END               0x0B64
#define GL_FOG_MODE              0x0B65
#define GL_FOG_COLOR             0x0B66
#define GL_DEPTH_TEST            0x0B71
#define GL_DEPTH_FUNC            0x0B74
#define GL_DEPTH_MASK            0x0B72
#define GL_DEPTH_CLEAR_VALUE     0x0B73
#define GL_TEXTURE_2D            0x0DE1
#define GL_LINE_SMOOTH           0x0B20
#define GL_POLYGON_SMOOTH        0x0B41
#define GL_POLYGON_OFFSET_FILL   0x8037
#define GL_POLYGON_OFFSET_POINT  0x2A01
#define GL_POLYGON_OFFSET_LINE   0x2A02
#define GL_POLYGON_OFFSET_FACTOR 0x8038
#define GL_POLYGON_OFFSET_UNITS  0x2A00
#define GL_POLYGON_MODE          0x0B40
#define GL_POINT                 0x1B00
#define GL_LINE                  0x1B01
#define GL_FILL                  0x1B02
#define GL_LINE_WIDTH            0x0B21

/* color masks / clear */
#define GL_COLOR_BUFFER_BIT   0x00004000
#define GL_DEPTH_BUFFER_BIT   0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400

/* matrices */
#define GL_MODELVIEW           0x1700
#define GL_PROJECTION          0x1701
#define GL_TEXTURE             0x1702
#define GL_MODELVIEW_MATRIX    0x0BA6
#define GL_PROJECTION_MATRIX   0x0BA7
#define GL_VIEWPORT            0x0BA2
#define GL_MODELVIEW_STACK_DEPTH  0x0D3A
#define GL_PROJECTION_STACK_DEPTH 0x0D38

/* GL version/strings */
#define GL_VERSION   0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_VENDOR    0x1F00
#define GL_RENDERER  0x1F01

/* client state */
#define GL_VERTEX_ARRAY         0x8074
#define GL_NORMAL_ARRAY         0x8075
#define GL_COLOR_ARRAY          0x8076
#define GL_TEXTURE_COORD_ARRAY  0x8078
#define GL_CLIENT_ALL_ATTRIB_BITS 0xFFFFFFFF

/* data types */
#define GL_BYTE           0x1400
#define GL_UNSIGNED_BYTE  0x1401
#define GL_SHORT          0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT            0x1404
#define GL_UNSIGNED_INT   0x1405
#define GL_FLOAT          0x1406
#define GL_2_BYTES        0x1407
#define GL_3_BYTES        0x1408
#define GL_4_BYTES        0x1409
#define GL_DOUBLE         0x140A

/* pixel formats */
#define GL_RED       0x1903
#define GL_ALPHA     0x1906
#define GL_RGB       0x1907
#define GL_RGBA      0x1908
#define GL_BGRA      0x80E1
#define GL_LUMINANCE     0x1909
#define GL_LUMINANCE_ALPHA 0x190A
#define GL_RGB5_A1    0x8057
#define GL_RGBA4      0x8056
#define GL_RGB565     0x8D62

#define GL_UNSIGNED_SHORT_5_6_5   0x8363
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_UNSIGNED_SHORT_5_5_5_1 0x8034

/* texture parameters */
#define GL_TEXTURE_WIDTH      0x1000
#define GL_TEXTURE_HEIGHT     0x1001
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S     0x2802
#define GL_TEXTURE_WRAP_T     0x2803
#define GL_NEAREST            0x2600
#define GL_LINEAR             0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST  0x2701
#define GL_NEAREST_MIPMAP_LINEAR  0x2702
#define GL_LINEAR_MIPMAP_LINEAR   0x2703
#define GL_REPEAT             0x2901
#define GL_CLAMP_TO_EDGE      0x812F
#define GL_CLAMP              0x2900
#define GL_TEXTURE_STACK_DEPTH 0x0D40

/* shading */
#define GL_FLAT        0x1D00
#define GL_SMOOTH      0x1D01
#define GL_SHADE_MODEL 0x0B54

/* materials / lights (unused, kept for compile) */
#define GL_AMBIENT            0x1200
#define GL_DIFFUSE            0x1201
#define GL_SPECULAR           0x1202
#define GL_POSITION           0x1203
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_COLOR_INDEXES      0x1603
#define GL_LIGHT0             0x4000
#define GL_LIGHT1             0x4001
#define GL_LIGHT2             0x4002
#define GL_LIGHT7             0x4007

/* VBO (GL 1.5) */
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW          0x88E4
#define GL_DYNAMIC_DRAW         0x88E8
#define GL_STREAM_DRAW          0x88E0
#define GL_BUFFER_SIZE          0x8764
#define GL_BUFFER_USAGE         0x8765

/* display lists */
#define GL_COMPILE           0x1300
#define GL_COMPILE_AND_EXECUTE 0x1301

/* hints */
#define GL_DONT_CARE                    0x1100
#define GL_FASTEST                      0x1101
#define GL_NICEST                       0x1102
#define GL_PERSPECTIVE_CORRECTION_HINT  0x0C50
#define GL_POINT_SMOOTH_HINT            0x0C51
#define GL_LINE_SMOOTH_HINT             0x0C52
#define GL_POLYGON_SMOOTH_HINT          0x0C53
#define GL_HINT_MODE                    0x0500

/* misc get */
#define GL_MAX_TEXTURE_SIZE  0x0D33
#define GL_COLOR 0x1800
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_ARRAY_BUFFER_BINDING 0x8894

/* il items (smooth) */
#define GL_CURRENT_COLOR 0x0B00

/* compression (not used on win32, kept for compile) */
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG 0x8C02
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG  0x8C00
#define GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG 0x8C03
#define GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG  0x8C01

#define GLEWAPI
#define GLAPI
#define GLEW_STATIC

#ifdef __cplusplus
}
#endif

#endif /* __GLEW_H_SHIM__ */