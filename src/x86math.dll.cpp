// implemented in Liberty https://github.com/HaydnTrigg/Liberty

//
// x86math.dll - x86 implementation of I3DMathEngine.
//
// Decompiled to match 052103_release_1149_Ipatch_ver1254.
// Reference source: Conquest/Liberty x86math.cpp (B. Baldwin, Digital Anvil).
//
// Single translation unit. Functions are emitted in target address order and
// named to objdiff's normalized target symbol names.
//

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef long GENRESULT;
typedef unsigned long U32;
typedef char C8;
typedef float SINGLE;

#define COMAPI __stdcall

#define GR_OK                    0
#define GR_GENERIC             (-1)
#define GR_INVALID_PARMS       (-2)
#define GR_INTERFACE_UNSUPPORTED (-3)

#define DACOM_LOW_PRIORITY 0x40000000

// This is an arbitrarily small constant used to circumvent matrix inversions
// that might overflow due to small determinants.
static const SINGLE MIN_DET = 1e-8f;

// ---------------------------------------------------------------------------
// Math types (layouts taken from the disassembly)
// ---------------------------------------------------------------------------

struct Vector
{
    SINGLE x, y, z;
};

struct Matrix
{
    SINGLE d[3][3];
};

struct Transform
{
    SINGLE d[3][3];
    Vector translation;
};

// ---------------------------------------------------------------------------
// DACOM interfaces (only the layout needed for vtable offsets is reproduced)
// ---------------------------------------------------------------------------

struct DACOMDESC
{
    U32  size;
    C8 * interface_name;
};

struct IDAComponent
{
    virtual GENRESULT COMAPI QueryInterface(const C8 *interface_name, void **instance) = 0;
    virtual U32       COMAPI AddRef(void) = 0;
    virtual U32       COMAPI Release(void) = 0;
};

struct IComponentFactory : public IDAComponent
{
    virtual GENRESULT COMAPI CreateInstance(DACOMDESC *descriptor, void **instance) = 0;
};

struct I3DMathEngine : public IDAComponent
{
    virtual GENRESULT COMAPI inverse(Matrix &dst, const Matrix &m) = 0;
    virtual GENRESULT COMAPI scale(Matrix &dst, const Matrix &m, SINGLE s) = 0;
    virtual SINGLE    COMAPI det(const Matrix &m) = 0;
};

struct ICOManager : public IDAComponent
{
    virtual GENRESULT COMAPI _reserved3(void) = 0;
    virtual GENRESULT COMAPI RegisterComponent(IComponentFactory *component, const C8 *interface_name, U32 priority) = 0;
};

// ---------------------------------------------------------------------------
// Inverse-square-root lookup table (Graphics Gems V, Ken Turkowski)
// ---------------------------------------------------------------------------

#define LOOKUP_BITS 9
#define EXP_POS     23
#define EXP_BIAS    127
#define LOOKUP_POS  (EXP_POS - LOOKUP_BITS)
#define SEED_POS    (EXP_POS - 8)
#define TABLE_SIZE  (2 << LOOKUP_BITS)

union _flint
{
    unsigned long i;
    float         f;
};

// ---------------------------------------------------------------------------
// Imports / globals
// ---------------------------------------------------------------------------

extern "C" __declspec(dllimport) int          __stdcall DisableThreadLibraryCalls(void *hLibModule);
extern "C" __declspec(dllimport) ICOManager * __stdcall DACOM_Acquire(void);

void *operator new(unsigned int size);

extern "C" void *DAComponent_x86MathEngine_I3DMathEngine_vtbl[];
extern "C" void *DAComponent_x86MathEngine_IComponentFactory_vtbl[];

#define IID_I3DMathEngine   "3DMathEngine"
#define implementation_name "x86"

unsigned char  byte_6F74030[TABLE_SIZE];   // inv_sqrt_obj.iSqrt
unsigned char  byte_6F74430;               // run-once guard
void          *pMathEngine;

// Forward declarations (single source file, target address order)
extern "C" void inv_sqrt_build_table(void);
extern "C" void inv_sqrt_register_atexit(void);
extern "C" void __cdecl inv_sqrt_atexit_thunk(void);

// The inv_sqrt_obj dynamic initializer keeps the constructor / atexit thunks as
// out-of-line calls; keep the optimizer from auto-inlining them together.
#pragma auto_inline(off)

// ---------------------------------------------------------------------------
// 0x6F71000 - dynamic initializer for inv_sqrt_obj
// ---------------------------------------------------------------------------
extern "C" void inv_sqrt_dynamic_init(void)
{
    inv_sqrt_build_table();
    inv_sqrt_register_atexit();
}

// ---------------------------------------------------------------------------
// 0x6F71010 - ISQRT::ISQRT (build inverse-square-root lookup table)
// ---------------------------------------------------------------------------
extern "C" void inv_sqrt_build_table(void)
{
    int f;
    union _flint fi, fo;

    for (f = 0; f < TABLE_SIZE; f++)
    {
        fi.i = ((EXP_BIAS - 1) << EXP_POS) | (f << LOOKUP_POS);
        fo.f = 1.0f / sqrt(fi.f);
        byte_6F74030[f] = (unsigned char)(((fo.i + (1 << (SEED_POS - 2))) >> SEED_POS) & 0xFF);
    }
    byte_6F74030[TABLE_SIZE / 2] = 0xFF;
}

// ---------------------------------------------------------------------------
// 0x6F71060 - register inv_sqrt_obj destructor with atexit
// ---------------------------------------------------------------------------
extern "C" void inv_sqrt_register_atexit(void)
{
    atexit(inv_sqrt_atexit_thunk);
}

// ---------------------------------------------------------------------------
// 0x6F71070 - inv_sqrt_obj atexit thunk
// ---------------------------------------------------------------------------
extern "C" void __cdecl inv_sqrt_atexit_thunk(void)
{
    if ((byte_6F74430 & 1) == 0)
        byte_6F74430 |= 1;
}

#pragma auto_inline(on)

// ---------------------------------------------------------------------------
// 0x6F71090 - DllMain
// ---------------------------------------------------------------------------
extern "C" int __stdcall DllMain(void *hinstDLL, unsigned int fdwReason, void *lpvReserved)
{
    switch (fdwReason)
    {
    case 1: // DLL_PROCESS_ATTACH
        {
            DisableThreadLibraryCalls(hinstDLL);

            void *self = ::operator new(8u);
            if (self)
            {
                ((void **)self)[0] = (void *)DAComponent_x86MathEngine_I3DMathEngine_vtbl;
                ((void **)self)[1] = (void *)DAComponent_x86MathEngine_IComponentFactory_vtbl;

                int f;
                union _flint fi, fo;
                for (f = 0; f < TABLE_SIZE; f++)
                {
                    fi.i = ((EXP_BIAS - 1) << EXP_POS) | (f << LOOKUP_POS);
                    fo.f = 1.0f / sqrt(fi.f);
                    byte_6F74030[f] = (unsigned char)(((fo.i + (1 << (SEED_POS - 2))) >> SEED_POS) & 0xFF);
                }
                byte_6F74030[TABLE_SIZE / 2] = 0xFF;
            }

            pMathEngine = self;

            if (pMathEngine)
            {
                ICOManager *DACOM = DACOM_Acquire();
                if (DACOM)
                {
                    IComponentFactory *factory =
                        pMathEngine ? (IComponentFactory *)((char *)pMathEngine + 4) : (IComponentFactory *)0;
                    DACOM->RegisterComponent(factory, IID_I3DMathEngine, DACOM_LOW_PRIORITY);
                }
            }
        }
        break;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// 0x6F71150 - x86MathEngine::QueryInterface
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_QueryInterface(void *self, const C8 *interface_name, void **instance)
{
    *instance = 0;
    return GR_GENERIC;
}

// ---------------------------------------------------------------------------
// 0x6F71160 - x86MathEngine::AddRef / Release (identical, folded)
// ---------------------------------------------------------------------------
extern "C" U32 __stdcall x86MathEngine_AddRef_Release(void *self)
{
    return 1;
}

// ---------------------------------------------------------------------------
// 0x6F71170 - x86MathEngine::IComponentFactory::CreateInstance
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_CreateInstance(IComponentFactory *self, DACOMDESC *descriptor, void **instance)
{
    DACOMDESC *info = descriptor;

    *instance = 0;

    if (info == 0 || info->interface_name == 0)
        return GR_INVALID_PARMS;

    if (info->size == sizeof(DACOMDESC) + 4 &&
        memcmp(IID_I3DMathEngine, info->interface_name, sizeof(IID_I3DMathEngine)) == 0 &&
        (((C8 **)info)[2] == 0 || stricmp(implementation_name, ((C8 **)info)[2]) == 0))
    {
        I3DMathEngine *engine = (I3DMathEngine *)((char *)self - 4);
        engine->AddRef();
        *instance = engine;
        return GR_OK;
    }

    return GR_INTERFACE_UNSUPPORTED;
}

// ---------------------------------------------------------------------------
// 0x6F711F0 - x86MathEngine::inverse
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_inverse(I3DMathEngine *self, Matrix &dst, const Matrix &m)
{
    GENRESULT result;

    const SINGLE determinant = self->det(m);

    if (fabs(determinant) > MIN_DET)
    {
        const SINGLE dt = 1.0f / determinant;

        dst.d[0][0] = (m.d[1][1] * m.d[2][2] - m.d[1][2] * m.d[2][1]) * dt;
        dst.d[1][0] = -(m.d[1][0] * m.d[2][2] - m.d[1][2] * m.d[2][0]) * dt;
        dst.d[2][0] = (m.d[1][0] * m.d[2][1] - m.d[1][1] * m.d[2][0]) * dt;
        dst.d[0][1] = -(m.d[0][1] * m.d[2][2] - m.d[0][2] * m.d[2][1]) * dt;
        dst.d[1][1] = (m.d[0][0] * m.d[2][2] - m.d[0][2] * m.d[2][0]) * dt;
        dst.d[2][1] = -(m.d[0][0] * m.d[2][1] - m.d[0][1] * m.d[2][0]) * dt;
        dst.d[0][2] = (m.d[0][1] * m.d[1][2] - m.d[0][2] * m.d[1][1]) * dt;
        dst.d[1][2] = -(m.d[0][0] * m.d[1][2] - m.d[0][2] * m.d[1][0]) * dt;
        dst.d[2][2] = (m.d[0][0] * m.d[1][1] - m.d[0][1] * m.d[1][0]) * dt;

        result = GR_OK;
    }
    else
    {
        // this is the adjoint which can still be usefull
        dst.d[0][0] = (m.d[1][1] * m.d[2][2] - m.d[1][2] * m.d[2][1]);
        dst.d[1][0] = -(m.d[1][0] * m.d[2][2] - m.d[1][2] * m.d[2][0]);
        dst.d[2][0] = (m.d[1][0] * m.d[2][1] - m.d[1][1] * m.d[2][0]);
        dst.d[0][1] = -(m.d[0][1] * m.d[2][2] - m.d[0][2] * m.d[2][1]);
        dst.d[1][1] = (m.d[0][0] * m.d[2][2] - m.d[0][2] * m.d[2][0]);
        dst.d[2][1] = -(m.d[0][0] * m.d[2][1] - m.d[0][1] * m.d[2][0]);
        dst.d[0][2] = (m.d[0][1] * m.d[1][2] - m.d[0][2] * m.d[1][1]);
        dst.d[1][2] = -(m.d[0][0] * m.d[1][2] - m.d[0][2] * m.d[1][0]);
        dst.d[2][2] = (m.d[0][0] * m.d[1][1] - m.d[0][1] * m.d[1][0]);

        result = GR_INVALID_PARMS;
    }

    return result;
}

// ---------------------------------------------------------------------------
// 0x6F71380 - x86MathEngine::general_inverse (assumes last row is 0 0 0 1)
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_general_inverse(void *self, Transform &dst, SINGLE &w, const Transform &t)
{
    GENRESULT result;

    const SINGLE determinant = t.d[0][0] * (t.d[1][1] * t.d[2][2] - t.d[2][1] * t.d[1][2]) -
                               t.d[0][1] * (t.d[1][0] * t.d[2][2] - t.d[2][0] * t.d[1][2]) +
                               t.d[0][2] * (t.d[1][0] * t.d[2][1] - t.d[2][0] * t.d[1][1]);

    SINGLE dt;
    if (fabs(determinant) > MIN_DET)
    {
        dt = 1.0f / determinant;
        result = GR_OK;
    }
    else
    {
        dt = 1.0f; // this will at least give us the adjoint
        result = GR_INVALID_PARMS;
    }

    dst.d[0][0] = (t.d[1][1] * t.d[2][2] - t.d[2][1] * t.d[1][2]) * dt;
    dst.d[0][1] = -(t.d[0][1] * t.d[2][2] - t.d[2][1] * t.d[0][2]) * dt;
    dst.d[0][2] = (t.d[0][1] * t.d[1][2] - t.d[1][1] * t.d[0][2]) * dt;

    dst.d[1][0] = -(t.d[1][0] * t.d[2][2] - t.d[2][0] * t.d[1][2]) * dt;
    dst.d[1][1] = (t.d[0][0] * t.d[2][2] - t.d[2][0] * t.d[0][2]) * dt;
    dst.d[1][2] = -(t.d[0][0] * t.d[1][2] - t.d[1][0] * t.d[0][2]) * dt;

    dst.d[2][0] = (t.d[1][0] * t.d[2][1] - t.d[2][0] * t.d[1][1]) * dt;
    dst.d[2][1] = -(t.d[0][0] * t.d[2][1] - t.d[2][0] * t.d[0][1]) * dt;
    dst.d[2][2] = (t.d[0][0] * t.d[1][1] - t.d[1][0] * t.d[0][1]) * dt;

    dst.translation.x = -(t.d[0][1] * t.d[1][2] * t.translation.z +
                          t.d[1][1] * t.d[2][2] * t.translation.x +
                          t.d[2][1] * t.d[0][2] * t.translation.y -
                          t.d[0][1] * t.d[2][2] * t.translation.y -
                          t.d[1][1] * t.d[0][2] * t.translation.z -
                          t.d[2][1] * t.d[1][2] * t.translation.x) * dt;

    dst.translation.y = (t.d[0][0] * t.d[1][2] * t.translation.z +
                         t.d[1][0] * t.d[2][2] * t.translation.x +
                         t.d[2][0] * t.d[0][2] * t.translation.y -
                         t.d[0][0] * t.d[2][2] * t.translation.y -
                         t.d[1][0] * t.d[0][2] * t.translation.z -
                         t.d[2][0] * t.d[1][2] * t.translation.x) * dt;

    dst.translation.z = -(t.d[0][0] * t.d[1][1] * t.translation.z +
                          t.d[1][0] * t.d[2][1] * t.translation.x +
                          t.d[2][0] * t.d[0][1] * t.translation.y -
                          t.d[0][0] * t.d[2][1] * t.translation.y -
                          t.d[1][0] * t.d[0][1] * t.translation.z -
                          t.d[2][0] * t.d[1][1] * t.translation.x) * dt;

    w = (t.d[0][0] * t.d[1][1] * t.d[2][2] +
         t.d[1][0] * t.d[2][1] * t.d[0][2] +
         t.d[2][0] * t.d[0][1] * t.d[1][2] -
         t.d[2][0] * t.d[1][1] * t.d[0][2] -
         t.d[0][0] * t.d[2][1] * t.d[1][2] -
         t.d[1][0] * t.d[0][1] * t.d[2][2]) * dt;

    return result;
}

// ---------------------------------------------------------------------------
// 0x6F715B0 - x86MathEngine::scale
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_scale(void *self, Matrix &dst, const Matrix &m, SINGLE s)
{
    dst.d[0][0] = m.d[0][0] * s;
    dst.d[0][1] = m.d[0][1] * s;
    dst.d[0][2] = m.d[0][2] * s;
    dst.d[1][0] = m.d[1][0] * s;
    dst.d[1][1] = m.d[1][1] * s;
    dst.d[1][2] = m.d[1][2] * s;
    dst.d[2][0] = m.d[2][0] * s;
    dst.d[2][1] = m.d[2][1] * s;
    dst.d[2][2] = m.d[2][2] * s;

    return GR_OK;
}

// ---------------------------------------------------------------------------
// 0x6F71620 - x86MathEngine::det
// ---------------------------------------------------------------------------
extern "C" SINGLE __stdcall x86MathEngine_det(void *self, const Matrix &m)
{
    return (m.d[0][0] * m.d[1][1] * m.d[2][2] +
            m.d[0][1] * m.d[1][2] * m.d[2][0] +
            m.d[0][2] * m.d[1][0] * m.d[2][1] -
            m.d[0][0] * m.d[1][2] * m.d[2][1] -
            m.d[0][1] * m.d[1][0] * m.d[2][2] -
            m.d[0][2] * m.d[1][1] * m.d[2][0]);
}

// ---------------------------------------------------------------------------
// 0x6F71670 - x86MathEngine::mul (Matrix)
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_mul_matrix(void *self, Matrix &dst, const Matrix &m1, const Matrix &m2)
{
    //FOR SOME UNKNOWN REASON, THE VC++ 5.0 OPTIMIZER CHOKES THE UNROLLED VERSION
    for (int i = 0; i < 3; i++)
    {
        dst.d[i][0] = m1.d[i][0] * m2.d[0][0] + m1.d[i][1] * m2.d[1][0] + m1.d[i][2] * m2.d[2][0];
        dst.d[i][1] = m1.d[i][0] * m2.d[0][1] + m1.d[i][1] * m2.d[1][1] + m1.d[i][2] * m2.d[2][1];
        dst.d[i][2] = m1.d[i][0] * m2.d[0][2] + m1.d[i][1] * m2.d[1][2] + m1.d[i][2] * m2.d[2][2];
    }

    return GR_OK;
}

// ---------------------------------------------------------------------------
// 0x6F716F0 - x86MathEngine::mul (Transform)
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_mul_transform(void *self, Transform &dst, const Transform &m1, const Transform &m2)
{
    dst.d[0][0] = m1.d[0][0] * m2.d[0][0] + m1.d[0][1] * m2.d[1][0] + m1.d[0][2] * m2.d[2][0];
    dst.d[0][1] = m1.d[0][0] * m2.d[0][1] + m1.d[0][1] * m2.d[1][1] + m1.d[0][2] * m2.d[2][1];
    dst.d[0][2] = m1.d[0][0] * m2.d[0][2] + m1.d[0][1] * m2.d[1][2] + m1.d[0][2] * m2.d[2][2];

    dst.translation.x = m1.d[0][0] * m2.translation.x + m1.d[0][1] * m2.translation.y + m1.d[0][2] * m2.translation.z + m1.translation.x;

    dst.d[1][0] = m1.d[1][0] * m2.d[0][0] + m1.d[1][1] * m2.d[1][0] + m1.d[1][2] * m2.d[2][0];
    dst.d[1][1] = m1.d[1][0] * m2.d[0][1] + m1.d[1][1] * m2.d[1][1] + m1.d[1][2] * m2.d[2][1];
    dst.d[1][2] = m1.d[1][0] * m2.d[0][2] + m1.d[1][1] * m2.d[1][2] + m1.d[1][2] * m2.d[2][2];

    dst.translation.y = m1.d[1][0] * m2.translation.x + m1.d[1][1] * m2.translation.y + m1.d[1][2] * m2.translation.z + m1.translation.y;

    dst.d[2][0] = m1.d[2][0] * m2.d[0][0] + m1.d[2][1] * m2.d[1][0] + m1.d[2][2] * m2.d[2][0];
    dst.d[2][1] = m1.d[2][0] * m2.d[0][1] + m1.d[2][1] * m2.d[1][1] + m1.d[2][2] * m2.d[2][1];
    dst.d[2][2] = m1.d[2][0] * m2.d[0][2] + m1.d[2][1] * m2.d[1][2] + m1.d[2][2] * m2.d[2][2];

    dst.translation.z = m1.d[2][0] * m2.translation.x + m1.d[2][1] * m2.translation.y + m1.d[2][2] * m2.translation.z + m1.translation.z;

    return GR_OK;
}

// ---------------------------------------------------------------------------
// 0x6F71830 - x86MathEngine::transform (Matrix) / rotate (identical, folded)
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_transform_vector(void *self, Vector &dst, const Matrix &m, const Vector &v)
{
    dst.x = m.d[0][0] * v.x + m.d[0][1] * v.y + m.d[0][2] * v.z;
    dst.y = m.d[1][0] * v.x + m.d[1][1] * v.y + m.d[1][2] * v.z;
    dst.z = m.d[2][0] * v.x + m.d[2][1] * v.y + m.d[2][2] * v.z;

    return GR_OK;
}

// ---------------------------------------------------------------------------
// 0x6F71890 - x86MathEngine::transform (Transform) - rotate a point and add the
// translation.  Column order matches the original per row (row 0: z,y,x; rows
// 1-2: z,x,y).
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_transform_point(void *self, Vector &dst, const Transform &m, const Vector &v)
{
    dst.x = m.d[0][2] * v.z + m.d[0][1] * v.y + m.d[0][0] * v.x + m.translation.x;
    dst.y = m.d[1][2] * v.z + m.d[1][0] * v.x + m.d[1][1] * v.y + m.translation.y;
    dst.z = m.d[2][2] * v.z + m.d[2][0] * v.x + m.d[2][1] * v.y + m.translation.z;

    return GR_OK;
}

// ---------------------------------------------------------------------------
// 0x6F718F0 - x86MathEngine::transform_transpose - rotate a vector by the
// transpose of the matrix (each result component uses a column of m).
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_transform_transpose(void *self, Vector &dst, const Matrix &m, const Vector &v)
{
    dst.x = m.d[2][0] * v.z + m.d[1][0] * v.y + m.d[0][0] * v.x;
    dst.y = m.d[2][1] * v.z + m.d[0][1] * v.x + m.d[1][1] * v.y;
    dst.z = m.d[2][2] * v.z + m.d[0][2] * v.x + m.d[1][2] * v.y;

    return GR_OK;
}

// ---------------------------------------------------------------------------
// 0x6F71950 - x86MathEngine::inverse_transform - subtract the translation then
// rotate by the transpose: dst = transpose(m) * (v - m.translation).
// ---------------------------------------------------------------------------
extern "C" GENRESULT __stdcall x86MathEngine_inverse_transform(void *self, Vector &dst, const Transform &m, const Vector &v)
{
    SINGLE dx = v.x - m.translation.x;
    SINGLE dy = v.y - m.translation.y;
    SINGLE dz = v.z - m.translation.z;

    dst.x = dz * m.d[2][0] + dy * m.d[1][0] + dx * m.d[0][0];
    dst.y = dz * m.d[2][1] + dy * m.d[1][1] + dx * m.d[0][1];
    dst.z = dz * m.d[2][2] + dy * m.d[1][2] + dx * m.d[0][2];

    return GR_OK;
}

// extern "C" prepends one underscore, so the C name carries one fewer than the
// target symbol (__delink_ida_const_start).
extern "C" char _delink_ida_const_start[];

// 0x6F71D60 - returns a fixed constant from the .rdata const pool, ignoring its
// argument (a degenerate/identity case among the dot-product helpers below).
extern "C" SINGLE __fastcall sub_6F71D60(const Vector *b, int, const Vector *a)
{
	return *(SINGLE *)&_delink_ida_const_start[0xC8];
}

// ---------------------------------------------------------------------------
// 0x6F71D70..0x6F71E40 - single-operand component accessors / sums on a Vector.
// Each loads one or more of the source vector's components onto the FPU stack.
// ---------------------------------------------------------------------------

// 0x6F71D70 - v.z
extern "C" SINGLE __stdcall sub_6F71D70(const Vector &v)
{
    return v.z;
}

// 0x6F71D90 - v.y
extern "C" SINGLE __stdcall sub_6F71D90(const Vector &v)
{
    return v.y;
}

// 0x6F71DA0 - v.z + v.y
extern "C" SINGLE __stdcall sub_6F71DA0(const Vector &v)
{
    return v.z + v.y;
}

// 0x6F71E00 - v.x
extern "C" SINGLE __stdcall sub_6F71E00(const Vector &v)
{
    return v.x;
}

// 0x6F71E10 - v.z + v.x
extern "C" SINGLE __stdcall sub_6F71E10(const Vector &v)
{
    return v.z + v.x;
}

// 0x6F71E30 - v.y + v.x
extern "C" SINGLE __stdcall sub_6F71E30(const Vector &v)
{
    return v.y + v.x;
}

// 0x6F71E40 - v.z + v.y + v.x
extern "C" SINGLE __stdcall sub_6F71E40(const Vector &v)
{
    return v.z + v.y + v.x;
}

// ---------------------------------------------------------------------------
// 0x6F71D80..0x6F71F10 - two-operand helpers: one component of vector a
// multiplied by the matching component of vector b (passed in ecx), with
// optional accumulation of further components of a.  a is the stack operand
// (loaded first), b stays in ecx.  The unused edx slot keeps b in ecx while
// pushing a onto the stack (__fastcall lowering of the original thiscall).
// ---------------------------------------------------------------------------

// 0x6F71D80 - a.z * b.z
extern "C" SINGLE __fastcall sub_6F71D80(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->z;
    return t * b->z;
}

// 0x6F71DB0 - a.z * b.z + a.y
extern "C" SINGLE __fastcall sub_6F71DB0(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->z;
    return t * b->z + a->y;
}

// 0x6F71DC0 - a.y * b.y
extern "C" SINGLE __fastcall sub_6F71DC0(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->y;
    return t * b->y;
}

// 0x6F71DD0 - a.y * b.y + a.z
extern "C" SINGLE __fastcall sub_6F71DD0(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->y;
    return t * b->y + a->z;
}

// 0x6F71E20 - a.z * b.z + a.x
extern "C" SINGLE __fastcall sub_6F71E20(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->z;
    return t * b->z + a->x;
}

// 0x6F71E50 - a.z * b.z + a.y + a.x
extern "C" SINGLE __fastcall sub_6F71E50(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->z;
    return t * b->z + a->y + a->x;
}

// 0x6F71E70 - a.y * b.y + a.x
extern "C" SINGLE __fastcall sub_6F71E70(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->y;
    return t * b->y + a->x;
}

// 0x6F71E80 - a.y * b.y + a.z + a.x
extern "C" SINGLE __fastcall sub_6F71E80(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->y;
    return t * b->y + a->z + a->x;
}

// 0x6F71EC0 - a.x * b.x
extern "C" SINGLE __fastcall sub_6F71EC0(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->x;
    return t * b->x;
}

// 0x6F71ED0 - a.x * b.x + a.z
extern "C" SINGLE __fastcall sub_6F71ED0(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->x;
    return t * b->x + a->z;
}

// 0x6F71F00 - a.x * b.x + a.y
extern "C" SINGLE __fastcall sub_6F71F00(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->x;
    return t * b->x + a->y;
}

// 0x6F71F10 - a.x * b.x + a.z + a.y
extern "C" SINGLE __fastcall sub_6F71F10(const Vector *b, int, const Vector *a)
{
    SINGLE t = a->x;
    return t * b->x + a->z + a->y;
}

// ---------------------------------------------------------------------------
// 0x6F71DE0..0x6F71F90 - sums of component products (partial dot products) of
// vectors a (stack) and b (ecx), with optional accumulation of a further raw
// component of a.  The a component of each product is read into a local first
// so it is loaded ahead of the b component (matching the original schedule).
// ---------------------------------------------------------------------------

// 0x6F71DE0 - a.z*b.z + a.y*b.y
extern "C" SINGLE __fastcall sub_6F71DE0(const Vector *b, int, const Vector *a)
{
    SINGLE z = a->z, y = a->y;
    return z * b->z + y * b->y;
}

// 0x6F71EA0 - a.z*b.z + a.y*b.y + a.x
extern "C" SINGLE __fastcall sub_6F71EA0(const Vector *b, int, const Vector *a)
{
    SINGLE z = a->z, y = a->y;
    return z * b->z + y * b->y + a->x;
}

// 0x6F71EE0 - a.z*b.z + a.x*b.x
extern "C" SINGLE __fastcall sub_6F71EE0(const Vector *b, int, const Vector *a)
{
    SINGLE z = a->z, x = a->x;
    return z * b->z + x * b->x;
}

// 0x6F71F30 - a.z*b.z + a.x*b.x + a.y
extern "C" SINGLE __fastcall sub_6F71F30(const Vector *b, int, const Vector *a)
{
    SINGLE z = a->z, x = a->x;
    return z * b->z + x * b->x + a->y;
}

// 0x6F71F50 - a.y*b.y + a.x*b.x
extern "C" SINGLE __fastcall sub_6F71F50(const Vector *b, int, const Vector *a)
{
    SINGLE y = a->y, x = a->x;
    return y * b->y + x * b->x;
}

// 0x6F71F70 - a.y*b.y + a.x*b.x + a.z
extern "C" SINGLE __fastcall sub_6F71F70(const Vector *b, int, const Vector *a)
{
    SINGLE y = a->y, x = a->x;
    return y * b->y + x * b->x + a->z;
}

// 0x6F71F90 - a.z*b.z + a.y*b.y + a.x*b.x  (full dot product)
extern "C" SINGLE __fastcall sub_6F71F90(const Vector *b, int, const Vector *a)
{
    SINGLE z = a->z, y = a->y, x = a->x;
    return z * b->z + y * b->y + x * b->x;
}
