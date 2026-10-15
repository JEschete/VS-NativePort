
typedef unsigned int size_t;
typedef unsigned long wchar_t;
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef struct 
{
  short m[3][3];
  long t[3];
} MATRIX;
typedef struct 
{
  long vx;
  long vy;
  long vz;
  long pad;
} VECTOR;
typedef struct 
{
  short vx;
  short vy;
  short vz;
  short pad;
} SVECTOR;
typedef struct 
{
  u_char r;
  u_char g;
  u_char b;
  u_char cd;
} CVECTOR;
typedef struct 
{
  short vx;
  short vy;
} DVECTOR;
typedef struct 
{
  SVECTOR v;
  VECTOR sxyz;
  DVECTOR sxy;
  CVECTOR rgb;
  short txuv;
  short pad;
  long chx;
  long chy;
} EVECTOR;
typedef struct 
{
  SVECTOR v;
  u_char uv[2];
  u_short pad;
  CVECTOR c;
  DVECTOR sxy;
  u_long sz;
} RVECTOR;
typedef struct 
{
  RVECTOR r01;
  RVECTOR r12;
  RVECTOR r20;
  RVECTOR *r0;
  RVECTOR *r1;
  RVECTOR *r2;
  u_long *rtn;
} CRVECTOR3;
typedef struct 
{
  u_long ndiv;
  u_long pih;
  u_long piv;
  u_short clut;
  u_short tpage;
  CVECTOR rgbc;
  u_long *ot;
  RVECTOR r0;
  RVECTOR r1;
  RVECTOR r2;
  CRVECTOR3 cr[5];
} DIVPOLYGON3;
typedef struct 
{
  RVECTOR r01;
  RVECTOR r02;
  RVECTOR r31;
  RVECTOR r32;
  RVECTOR rc;
  RVECTOR *r0;
  RVECTOR *r1;
  RVECTOR *r2;
  RVECTOR *r3;
  u_long *rtn;
} CRVECTOR4;
typedef struct 
{
  u_long ndiv;
  u_long pih;
  u_long piv;
  u_short clut;
  u_short tpage;
  CVECTOR rgbc;
  u_long *ot;
  RVECTOR r0;
  RVECTOR r1;
  RVECTOR r2;
  RVECTOR r3;
  CRVECTOR4 cr[5];
} DIVPOLYGON4;
typedef struct 
{
  short xy[3];
  short uv[2];
  short rgb[3];
} SPOL;
typedef struct 
{
  short sxy[4][2];
  short sz[4][2];
  short uv[4][2];
  short rgb[4][3];
  short code;
} POL4;
typedef struct 
{
  short sxy[3][2];
  short sz[3][2];
  short uv[3][2];
  short rgb[3][3];
  short code;
} POL3;
typedef struct 
{
  SVECTOR *v;
  SVECTOR *n;
  SVECTOR *u;
  CVECTOR *c;
  u_long len;
} TMESH;
typedef struct 
{
  SVECTOR *v;
  SVECTOR *n;
  SVECTOR *u;
  CVECTOR *c;
  u_long lenv;
  u_long lenh;
} QMESH;
extern void InitGeom();
extern void EigenMatrix(MATRIX *m, MATRIX *t);
extern int IsIdMatrix(MATRIX *m);
extern MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
extern MATRIX *MulRotMatrix0(MATRIX *m0, MATRIX *m1);
extern MATRIX *MulMatrix(MATRIX *m0, MATRIX *m1);
extern MATRIX *MulMatrix2(MATRIX *m0, MATRIX *m1);
extern MATRIX *MulRotMatrix(MATRIX *m0);
extern MATRIX *SetMulMatrix(MATRIX *m0, MATRIX *m1);
extern MATRIX *SetMulRotMatrix(MATRIX *m0);
extern VECTOR *ApplyMatrix(MATRIX *m, SVECTOR *v0, VECTOR *v1);
extern VECTOR *ApplyRotMatrix(SVECTOR *v0, VECTOR *v1);
extern VECTOR *ApplyRotMatrixLV(VECTOR *v0, VECTOR *v1);
extern VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
extern SVECTOR *ApplyMatrixSV(MATRIX *m, SVECTOR *v0, SVECTOR *v1);
extern VECTOR *ApplyTransposeMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
extern MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrixXZY(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrixYZX(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrixZXY(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrixZYX(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrix_gte(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrixYXZ_gte(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrixZYX_gte(SVECTOR *r, MATRIX *m);
extern MATRIX *RotMatrixX(long r, MATRIX *m);
extern MATRIX *RotMatrixY(long r, MATRIX *m);
extern MATRIX *RotMatrixZ(long r, MATRIX *m);
extern MATRIX *RotMatrixC(SVECTOR *r, MATRIX *m);
extern MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
extern MATRIX *ScaleMatrix(MATRIX *m, VECTOR *v);
extern MATRIX *ScaleMatrixL(MATRIX *m, VECTOR *v);
extern MATRIX *TransposeMatrix(MATRIX *m0, MATRIX *m1);
extern MATRIX *CompMatrix(MATRIX *m0, MATRIX *m1, MATRIX *m2);
extern MATRIX *CompMatrixLV(MATRIX *m0, MATRIX *m1, MATRIX *m2);
extern void MatrixNormal(MATRIX *m, MATRIX *n);
extern void MatrixNormal_0(MATRIX *m, MATRIX *n);
extern void MatrixNormal_1(MATRIX *m, MATRIX *n);
extern void MatrixNormal_2(MATRIX *m, MATRIX *n);
extern void SetRotMatrix(MATRIX *m);
extern void SetLightMatrix(MATRIX *m);
extern void SetColorMatrix(MATRIX *m);
extern void SetTransMatrix(MATRIX *m);
extern void PushMatrix();
extern void PopMatrix();
extern void ReadRotMatrix(MATRIX *m);
extern void ReadLightMatrix(MATRIX *m);
extern void ReadColorMatrix(MATRIX *m);
extern void SetRGBcd(CVECTOR *v);
extern void SetBackColor(long rbk, long gbk, long bbk);
extern void SetFarColor(long rfc, long gfc, long bfc);
extern void SetGeomOffset(long ofx, long ofy);
extern void SetGeomScreen(long h);
extern void ReadSZfifo3(long *sz0, long *sz1, long *sz2);
extern void ReadSZfifo4(long *szx, long *sz0, long *sz1, long *sz2);
extern void ReadSXSYfifo(long *sxy0, long *sxy1, long *sxy2);
extern void ReadRGBfifo(CVECTOR *v0, CVECTOR *v1, CVECTOR *v2);
extern void ReadGeomOffset(long *ofx, long *ofy);
extern long ReadGeomScreen();
extern void TransRot_32(VECTOR *v0, VECTOR *v1, long *flag);
extern long TransRotPers(SVECTOR *v0, long *sxy, long *p, long *flag);
extern long TransRotPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2, long *p, long *flag);
extern void pers_map(int abuf, SVECTOR **vertex, int tex[4][2], u_short *dtext);
extern void PhongLine(int istart_x, int iend_x, int p, int q, u_short **pixx, int fs, int ft, int i4, int det);
extern long RotTransPers(SVECTOR *v0, long *sxy, long *p, long *flag);
extern long RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2, long *p, long *flag);
extern void RotTrans(SVECTOR *v0, VECTOR *v1, long *flag);
extern void RotTransSV(SVECTOR *v0, SVECTOR *v1, long *flag);
extern void LocalLight(SVECTOR *v0, VECTOR *v1);
extern void LightColor(VECTOR *v0, VECTOR *v1);
extern void DpqColorLight(VECTOR *v0, CVECTOR *v1, long p, CVECTOR *v2);
extern void DpqColor(CVECTOR *v0, long p, CVECTOR *v1);
extern void DpqColor3(CVECTOR *v0, CVECTOR *v1, CVECTOR *v2, long p, CVECTOR *v3, CVECTOR *v4, CVECTOR *v5);
extern void Intpl(VECTOR *v0, long p, CVECTOR *v1);
extern VECTOR *Square12(VECTOR *v0, VECTOR *v1);
extern VECTOR *Square0(VECTOR *v0, VECTOR *v1);
extern VECTOR *SquareSL12(SVECTOR *v0, VECTOR *v1);
extern VECTOR *SquareSL0(SVECTOR *v0, VECTOR *v1);
extern SVECTOR *SquareSS12(SVECTOR *v0, SVECTOR *v1);
extern SVECTOR *SquareSS0(SVECTOR *v0, SVECTOR *v1);
extern void NormalColor(SVECTOR *v0, CVECTOR *v1);
extern void NormalColor3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, CVECTOR *v4, CVECTOR *v5);
extern void NormalColorDpq(SVECTOR *v0, CVECTOR *v1, long p, CVECTOR *v2);
extern void NormalColorDpq3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, long p, CVECTOR *v4, CVECTOR *v5, CVECTOR *v6);
extern void NormalColorCol(SVECTOR *v0, CVECTOR *v1, CVECTOR *v2);
extern void NormalColorCol3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, CVECTOR *v4, CVECTOR *v5, CVECTOR *v6);
extern void ColorDpq(VECTOR *v0, CVECTOR *v1, long p, CVECTOR *v2);
extern void ColorCol(VECTOR *v0, CVECTOR *v1, CVECTOR *v2);
extern long NormalClip(long sxy0, long sxy1, long sxy2);
extern long AverageZ3(long sz0, long sz1, long sz2);
extern long AverageZ4(long sz0, long sz1, long sz2, long sz3);
extern void OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2);
extern void OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);
extern long Lzc(long data);
extern long RotTransPers4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1, long *sxy2, long *sxy3, long *p, long *flag);
extern void RotTransPersN(SVECTOR *v0, DVECTOR *v1, u_short *sz, u_short *p, u_short *flag, long n);
extern void RotTransPers3N(SVECTOR *v0, DVECTOR *v1, u_short *sz, u_short *flag, long n);
extern void RotMeshH(short *Yheight, DVECTOR *Vo, u_short *sz, u_short *flag, short Xoffset, short Zoffset, short m, short n, DVECTOR *base);
extern long RotAverage3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2, long *p, long *flag);
extern long RotAverage4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1, long *sxy2, long *sxy3, long *p, long *flag);
extern long RotNclip3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2, long *p, long *otz, long *flag);
extern long RotNclip4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1, long *sxy2, long *sxy3, long *p, long *otz, long *flag);
extern long RotAverageNclip3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, long *sxy0, long *sxy1, long *sxy2, long *p, long *otz, long *flag);
extern long RotAverageNclip4(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, long *sxy0, long *sxy1, long *sxy2, long *sxy3, long *p, long *otz, long *flag);
extern long RotColorDpq(SVECTOR *v0, SVECTOR *v1, CVECTOR *v2, long *sxy, CVECTOR *v3, long *flag);
extern long RotColorDpq3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, SVECTOR *v4, SVECTOR *v5, CVECTOR *v6, long *sxy0, long *sxy1, long *sxy2, CVECTOR *v7, CVECTOR *v8, CVECTOR *v9, long *flag);
extern long RotAverageNclipColorDpq3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, SVECTOR *v4, SVECTOR *v5, CVECTOR *v6, long *sxy0, long *sxy1, long *sxy2, CVECTOR *v7, CVECTOR *v8, CVECTOR *v9, long *otz, long *flag);
extern long RotAverageNclipColorCol3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, SVECTOR *v4, SVECTOR *v5, CVECTOR *v6, long *sxy0, long *sxy1, long *sxy2, CVECTOR *v7, CVECTOR *v8, CVECTOR *v9, long *otz, long *flag);
extern long RotColorMatDpq(SVECTOR *v0, SVECTOR *v1, CVECTOR *v2, long *sxy, CVECTOR *v3, long matc, long flag);
extern void ColorMatDpq(SVECTOR *v0, CVECTOR *v1, long p, CVECTOR *v2, long matc);
extern void ColorMatCol(SVECTOR *v0, CVECTOR *v1, CVECTOR *v2, long matc);
extern void LoadAverage12(VECTOR *v0, VECTOR *v1, long p0, long p1, VECTOR *v2);
extern void LoadAverageShort12(SVECTOR *v0, SVECTOR *v1, long p0, long p1, SVECTOR *v2);
extern void LoadAverage0(VECTOR *v0, VECTOR *v1, long p0, long p1, VECTOR *v2);
extern void LoadAverageShort0(SVECTOR *v0, SVECTOR *v1, long p0, long p1, SVECTOR *v2);
extern void LoadAverageByte(u_char *v0, u_char *v1, long p0, long p1, u_char *v2);
extern void LoadAverageCol(u_char *v0, u_char *v1, long p0, long p1, u_char *v2);
extern long VectorNormal(VECTOR *v0, VECTOR *v1);
extern long VectorNormalS(VECTOR *v0, SVECTOR *v1);
extern long VectorNormalSS(SVECTOR *v0, SVECTOR *v1);
extern long SquareRoot0(long a);
extern long SquareRoot12(long a);
extern void InvSquareRoot(long a, long *b, long *c);
extern void gteMIMefunc(SVECTOR *otp, SVECTOR *dfp, long n, long p);
extern void SetFogFar(long a, long h);
extern void SetFogNear(long a, long h);
extern void SetFogNearFar(long a, long b, long h);
extern void SubPol4(POL4 *p, SPOL *sp, int ndiv);
extern void SubPol3(POL3 *p, SPOL *sp, int ndiv);
extern int rcos(int a);
extern int rsin(int a);
extern int ccos(int a);
extern int csin(int a);
extern int cln(int a);
extern int csqrt(int a);
extern int catan(int a);
extern long ratan2(long y, long x);
extern void RotPMD_F3(long *pa, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_G3(long *pa, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_FT3(long *pa, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_GT3(long *pa, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_F4(long *pa, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_G4(long *pa, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_FT4(long *pa, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_GT4(long *pa, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_SV_F3(long *pa, long *va, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_SV_G3(long *pa, long *va, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_SV_FT3(long *pa, long *va, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_SV_GT3(long *pa, long *va, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_SV_F4(long *pa, long *va, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_SV_G4(long *pa, long *va, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_SV_FT4(long *pa, long *va, u_long *ot, int otlen, int id, int backc);
extern void RotPMD_SV_GT4(long *pa, long *va, u_long *ot, int otlen, int id, int backc);
extern void InitClip(EVECTOR *evbfad, long hw, long vw, long h, long near, long far);
extern long Clip3F(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, EVECTOR **evmx);
extern long Clip3FP(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, EVECTOR **evmx);
extern long Clip4F(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, EVECTOR **evmx);
extern long Clip4FP(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, EVECTOR **evmx);
extern long Clip3FT(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, short *uv0, short *uv1, short *uv2, EVECTOR **evmx);
extern long Clip3FTP(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, short *uv0, short *uv1, short *uv2, EVECTOR **evmx);
extern long Clip4FT(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, short *uv0, short *uv1, short *uv2, short *uv3, EVECTOR **evmx);
extern long Clip4FTP(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, short *uv0, short *uv1, short *uv2, short *uv3, EVECTOR **evmx);
extern long Clip3G(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *rgb0, CVECTOR *rgb1, CVECTOR *rgb2, EVECTOR **evmx);
extern long Clip3GP(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *rgb0, CVECTOR *rgb1, CVECTOR *rgb2, EVECTOR **evmx);
extern long Clip4G(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, CVECTOR *rgb0, CVECTOR *rgb1, CVECTOR *rgb2, CVECTOR *rgb3, EVECTOR **evmx);
extern long Clip4GP(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, CVECTOR *rgb0, CVECTOR *rgb1, CVECTOR *rgb2, CVECTOR *rgb3, EVECTOR **evmx);
extern long Clip3GT(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, short *uv0, short *uv1, short *uv2, CVECTOR *rgb0, CVECTOR *rgb1, CVECTOR *rgb2, EVECTOR **evmx);
extern long Clip3GTP(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, short *uv0, short *uv1, short *uv2, CVECTOR *rgb0, CVECTOR *rgb1, CVECTOR *rgb2, EVECTOR **evmx);
extern long Clip4GT(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, short *uv0, short *uv1, short *uv2, short *uv3, CVECTOR *rgb0, CVECTOR *rgb1, CVECTOR *rgb2, CVECTOR *rgb3, EVECTOR **evmx);
extern long Clip4GTP(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, short *uv0, short *uv1, short *uv2, short *uv3, CVECTOR *rgb0, CVECTOR *rgb1, CVECTOR *rgb2, CVECTOR *rgb3, EVECTOR **evmx);
extern void RotTransPers_nom(SVECTOR *v0);
extern void RotTransPers3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2);
extern void RotTransPers4_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3);
extern void RotTrans_nom(SVECTOR *v0);
extern void RotAverage3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2);
extern void RotNclip3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2);
extern void RotAverageNclip3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2);
extern void RotAverageNclipColorDpq3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, SVECTOR *v4, SVECTOR *v5, CVECTOR *v6);
extern void RotAverageNclipColorCol3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, SVECTOR *v4, SVECTOR *v5, CVECTOR *v6);
extern void RotColorDpq_nom(SVECTOR *v0, SVECTOR *v1, CVECTOR *v2);
extern long RotColorDpq3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, SVECTOR *v3, SVECTOR *v4, SVECTOR *v5, CVECTOR *v6);
extern void NormalColor_nom(SVECTOR *v0);
extern void NormalColor3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2);
extern void NormalColorDpq_nom(SVECTOR *v0, CVECTOR *v1, long p);
extern void NormalColorDpq3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3, long p);
extern void NormalColorCol_nom(SVECTOR *v0, CVECTOR *v1);
extern void NormalColorCol3_nom(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, CVECTOR *v3);
extern void RotSMD_F3(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_G3(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_FT3(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_GT3(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_F4(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_G4(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_FT4(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_GT4(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_SV_F3(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_SV_G3(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_SV_FT3(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_SV_GT3(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_SV_F4(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_SV_G4(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_SV_FT4(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotSMD_SV_GT4(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_F3(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_G3(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_FT3(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_GT3(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_F4(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_G4(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_FT4(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_GT4(long *pa, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_SV_F3(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_SV_G3(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_SV_FT3(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_SV_GT3(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_SV_F4(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_SV_G4(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_SV_FT4(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern void RotRMD_SV_GT4(long *pa, long *va, u_long *ot, int otlen, int id, int sclip, int hclip, int vclip, int nclipmode);
extern long p2otz(long p, long projection);
extern long otz2p(long otz, long projection);
typedef struct 
{
  u_int unk0_0 : 8;
  u_int unk0_8 : 8;
  u_int unk0_16 : 8;
  u_int unk0_24 : 8;
} func_8006EBF8_t_fields;
typedef struct 
{
  union 
  {
    func_8006EBF8_t_fields fields;
    int value;
  } unk0;
  SVECTOR unk4;
} func_8006EBF8_t3;
typedef struct 
{
  func_8006EBF8_t3 unk0;
  int unkC;
} func_8006EBF8_t;
typedef struct 
{
  char unk0_0;
  char unk0_8;
  char unk0_16;
  u_int unk0_24 : 8;
} func_80089888_t;
typedef struct 
{
  u_char actorId : 3;
  u_char unk0_3 : 1;
  u_char unk0_4 : 2;
  u_char unk0_7 : 2;
  u_char unk1;
  u_char unk2;
  u_char unk3;
  u_char unk4;
  u_char unk5;
  u_char unk6;
  u_char unk7;
  int unk8;
} D_800F4538_t2;
typedef struct 
{
  signed char unk0;
  signed char unk1;
  signed char unk2;
  u_char unk3;
} D_800F4538_unk58;
typedef struct 
{
  u_char unk0;
  u_char unk1;
  u_char unk2;
  u_char unk3;
} D_800F4538_unk58_2;
typedef struct 
{
  int unk0;
  u_char unk4;
  u_char unk5;
  u_char unk6;
  u_char unk7;
  u_short unk8;
  short unkA;
  u_short unkC;
  u_short unkE;
  short unk10;
  short unk12;
} D_800F4538_unk1864;
typedef struct 
{
  SVECTOR unk0[42];
  SVECTOR unk150[42];
  SVECTOR unk2A0[42];
  SVECTOR unk3F0[42];
  u_short unk540;
  u_short unk542;
  u_char *unk544;
  u_char animationId;
  u_char unk549;
  int unk548_16 : 1;
  int unk548_17 : 1;
  int unk548_18 : 14;
  int unk54C;
} D_800F4538_unkC54;
typedef struct 
{
  int unk0;
  u_char unk4;
  u_char unk5;
  u_char unk6;
  u_char unk7;
  u_char unk8;
  u_char unk9;
  u_short unkA;
  u_char unkC;
  u_char unkD;
  u_char unkE;
  u_char unkF;
} _armature_t;
typedef struct 
{
  u_char unk0[48];
  void *akaoOffset;
  void *groupOffset;
  void *vertexOffset;
  void *polyOffset;
  _armature_t armatures[0];
} D_800F4538_unk68;
typedef struct 
{
  u_char unk0;
  u_char unk1;
  u_char unk2;
  u_char unk3;
} D_800F4538_unk64;
typedef struct 
{
  u_char unk0;
  u_char unk1;
  u_char unk2;
  u_char unk3;
  short unk4;
  short unk6;
  u_int unk8;
  int unkC;
  SVECTOR unk10;
} D_800F4538_unk1848;
typedef struct 
{
  u_char unk0[0x30];
  int texturesOffset;
  int vertexGroupsOffset;
  int verticesOffset;
  int polygonsOffset;
  u_char unk40[0x7C0];
} _wepModelOffsets;
typedef struct 
{
  u_char nBones;
  u_char nVertexGroups;
  u_short nTriangles;
  u_short nQuads;
  u_short nOther;
  char skip : 1;
  char visible : 1;
  char freeze : 1;
  char unk8_3 : 1;
  char unk8_4 : 3;
  char unk8_7 : 1;
  u_char unk9_0 : 4;
  u_char unk9_4 : 1;
  u_char unk9_5 : 1;
  u_char unk9_6 : 1;
  u_char unk9_7 : 1;
  u_char unkA_0 : 3;
  u_char unkA_3 : 2;
  u_char unkA_5 : 1;
  u_char unkA_6 : 1;
  u_char weaponDrawn : 1;
  u_char unkB_0 : 4;
  u_char unkB_4 : 3;
  u_char unkB_7 : 1;
  u_char unkC_0 : 4;
  u_char unkC_4 : 4;
  u_char unkD;
  u_char unkE;
  u_char unkF;
  u_char unk10;
  u_char unk11;
  u_char unk12;
  u_char unk13;
  u_short unk14;
  u_short unk16;
  u_short unk18;
  u_short unk1A;
  SVECTOR position;
  short unk24;
  short facing;
  short unk28;
  short unk2A;
  short unk2C;
  short unk2E;
  short unk30;
  short unk32;
  SVECTOR unk34;
  short unk3C;
  short unk3E;
  short unk40;
  u_short unk42;
  SVECTOR unk44;
  int unk4C;
  short unk50;
  short unk52;
  CVECTOR unk54;
  D_800F4538_unk58_2 unk58;
  u_char currentTileX;
  u_char unk5D;
  u_char currentTileZ;
  u_char unk5F;
  u_char lastTouchedTileX;
  u_char unk61;
  u_char lastTouchedTileZ;
  u_char unk63;
  D_800F4538_unk64 unk64;
  D_800F4538_unk68 *unk68;
} D_800F4538_unk0;
typedef struct 
{
  u_char unk0_0 : 4;
  u_char unk0_4 : 4;
  u_char unk1;
  u_char unk2;
  u_char unk3;
} D_800F4538_unk180C;
typedef struct 
{
  u_char x;
  u_char y;
  u_char w;
  u_char h;
} texMapOverlay_t;
typedef struct 
{
  u_char x;
  u_char y;
  u_char columns;
  u_char unk3;
} D_800F4538_unk614;
typedef struct 
{
  u_char overlay;
  u_char x;
  u_char y;
  u_char columns;
} texMapAnimation_t;
typedef struct 
{
  u_char x;
  u_char y;
  u_char w;
  u_char h;
  u_char unk4;
  u_char unk5;
  u_char unk6_0 : 1;
  u_char unk6_1 : 7;
  u_char unk7;
} D_800F4538_unk61C;
typedef struct 
{
  u_char x;
  u_char y;
  signed char dx;
  signed char dy;
  u_char interval;
  u_char timer;
  u_char unk6;
  u_char unk7;
} D_800F4538_unk181C;
typedef struct 
{
  D_800F4538_unk0 unk0;
  MATRIX bones[42];
  u_char unk5AC_0 : 2;
  u_char unk5AC_2 : 1;
  u_char unk5AC_3 : 1;
  u_char unk5AC_4 : 2;
  u_char unk5AC_6 : 1;
  u_char momentumStop : 1;
  u_char unk5AC_8 : 1;
  u_char unk5AC_9 : 1;
  u_char unk5AC_10 : 1;
  u_char unk5AC_11 : 1;
  u_char unk5AC_12 : 1;
  u_char unk5AC_13 : 1;
  u_char unk5AC_14 : 1;
  u_char unk5AC_15 : 1;
  u_char unk5AC_16 : 1;
  u_char unk5AC_17 : 1;
  u_char unk5AC_18 : 1;
  u_char unk5AC_19 : 1;
  u_char unk5AC_20 : 1;
  u_char unk5AC_21 : 1;
  u_char unk5AC_22 : 1;
  u_char unk5AC_23 : 1;
  u_char unk5AC_24 : 1;
  u_char unk5AC_25 : 1;
  u_char unk5AC_26 : 1;
  u_char unk5AC_27 : 1;
  u_char unk5AC_28 : 4;
  u_char unk5B0_0 : 2;
  u_char unk5B0_2 : 1;
  u_char unk5B0_3 : 1;
  u_char unk5B0_4 : 1;
  u_char unk5B0_5 : 1;
  u_char unk5B0_6 : 2;
  u_char unk5B1;
  u_char unk5B2;
  u_char unk5B3;
  u_char unk5B4;
  u_char unk5B5;
  u_char unk5B6;
  u_char unk5B7;
  u_char animationId;
  u_char unk5B9;
  u_char unk5BA;
  u_char unk5BB;
  short unk5BC;
  short unk5BE;
  u_short unk5C0;
  u_short unk5C2;
  u_short unk5C4;
  u_short unk5C6;
  u_short unk5C8;
  u_short unk5CA;
  char unk5CC;
  char unk5CD;
  short unk5CE;
  void *unk5D0;
  void *unk5D4;
  void *unk5D8;
  void *unk5DC[4];
  SVECTOR unk5EC;
  texMapOverlay_t texMapOverlays[8];
  D_800F4538_unk614 unk614[2];
  D_800F4538_unk61C unk61C[4];
  u_short unk63C;
  u_short menuCameraHeightOffset;
  u_short unk640;
  u_short unk642;
  u_short unk644;
  u_short unk646;
  u_int unk648_0 : 16;
  u_int unk648_16 : 1;
  u_int unk648_17 : 15;
  u_short shadowRadius;
  u_short shadowRadiusExpandRate;
  u_short shadowRadiusShrinkRate;
  u_short unk652;
  u_short unk654;
  u_short menuCameraDistance;
  u_short unk658;
  short unk65A;
  int unk65C;
  int unk660;
  u_int btSeqLbas[12];
  u_short chainAttackAnimationIds[12];
  u_int spSeqLbas[4];
  int unk6BC[4];
  u_short spSeqSizes[4];
  int unk6D4[2];
  int unk6DC;
  u_char unk6E0;
  u_char unk6E1;
  u_char unk6E2;
  u_char unk6E3;
  u_char unk6E4;
  u_char unk6E5;
  u_short unk6E6;
  short unk6E8;
  short unk6EA;
  short unk6EC;
  signed char unk6EE;
  signed char unk6EF;
  u_char unk6F0;
  u_char unk6F1;
  u_char unk6F2;
  u_char unk6F3;
  D_800F4538_unk58 unk6F4;
  D_800F4538_unk58_2 unk6F8;
  SVECTOR unk6FC;
  D_800F4538_unkC54 unk704;
  D_800F4538_unkC54 unkC54;
  u_char unk11A4[0x280];
  u_short unk1424[0x140];
  u_short unk16A4[0xA0];
  D_800F4538_unk64 unk17E4;
  D_800F4538_unk64 unk17E8;
  SVECTOR unk17EC;
  SVECTOR unk17F4;
  signed char unk17FC;
  u_char unk17FD;
  u_char unk17FE;
  u_char unk17FF;
  u_short unk1800;
  u_char unk1802[2];
  texMapAnimation_t unk1804[2];
  D_800F4538_unk180C unk180C[2];
  short unk1814;
  short unk1816;
  short unk1818;
  short unk181A;
  D_800F4538_unk181C unk181C[4];
  short unk183C;
  u_char unk183E[2];
  short unk1840;
  short unk1842;
  short unk1844;
  u_char unk1846;
  u_char unk1847;
  D_800F4538_unk1848 unk1848;
  u_char unk1860;
  u_char unk1861;
  u_char unk1862;
  u_char unk1863;
  D_800F4538_unk1864 unk1864;
  int unk1868;
  int unk187C;
  u_char unk1880[0x10];
  MATRIX unk1890[2];
  u_char unk18D0[2];
  u_short unk18D2;
  u_short unk18D4;
  u_short unk18D6;
  u_short unk18D8;
} D_800F4538_t;
typedef struct 
{
  u_char nBones;
  u_char nVertexGroups;
  u_short nTriangles;
  u_short nQuads;
  u_short nOther;
  u_char actorId : 4;
  u_char unk8_4 : 1;
  u_char isWideTexture : 1;
  u_char unk8_6 : 1;
  u_char isDoubleClut : 1;
  u_char unk9_0 : 4;
  u_char material : 4;
  u_char unkA_0 : 7;
  u_char unkA_7 : 1;
  u_char nClutColors;
  u_char unkC;
  u_char unkD;
  u_char modelId;
  u_char texSlot;
  u_char clutSlotOffset;
  u_char unk11;
  u_char unk12;
  u_char unk13;
  int unk14;
  short unk18;
  short unk1A;
  _wepModelOffsets *offsets;
  MATRIX unk20[4];
  SVECTOR unkBoneInfo[4];
  u_short palettes[7][64];
  u_short unk440[96];
  u_short unk500[96];
  u_char unk5C0;
  u_char unk5C1;
  u_short unk5C2;
  int unk5C4;
  int unk5C8;
  char unk5CC;
  char unk5CD;
  short unk5CE;
  int unk5D0;
  int unk5D4;
  void *unk5D8;
  int unk5DC;
  int unk5E0;
  int unk5E4;
} vs_battle_wepModels_t;
typedef struct 
{
  char unk0;
  char unk1;
  char unk2;
  char unk3;
  int unk4;
  char unk8_0 : 1;
  char unk8_1 : 1;
  char unk8_2 : 1;
  char unk8_3 : 1;
  char unk8_4 : 1;
  char unk8_5 : 1;
  char unk8_6 : 1;
  char unk8_7 : 1;
  u_char unk9_0 : 4;
  u_char unk9_4 : 4;
  u_char unkA_0 : 3;
  u_char unkA_3 : 2;
  u_char unkA_5 : 2;
  u_char unkA_7 : 1;
  u_char unkB_0 : 4;
  u_char unkB_4 : 3;
  u_char unkB_7 : 1;
  u_char unkC_0 : 4;
  u_char unkC_4 : 4;
  u_char unkD;
  u_char unkE;
  u_char unkF;
  u_char unk10;
  u_char unk11;
  u_char unk12;
  u_char unk13;
  int unk14;
  short unk18;
  short unk1A;
  short unk1C;
  short unk1E;
  short unk20;
  short unk22;
  short unk24;
  u_short unk26;
  int unk28;
  short unk2C;
  short unk2E;
  short unk30;
  short unk32;
  short unk34;
  short unk36;
  short unk38;
  short unk3A;
  int unk3C;
  int unk40;
  int unk44;
  int unk48;
  int unk4C;
  short unk50;
  short unk52;
  int unk54;
  D_800F4538_unk58_2 unk58;
  char unk5C;
  volatile char unk5D;
  char unk5E;
  char unk5F;
  int unk60;
  D_800F4538_unk64 unk64;
  D_800F4538_unk68 *unk68;
  MATRIX unk6C[5];
  SVECTOR unk10C;
  u_char unk114[0x18];
  u_short unk12C[16];
  u_char unk14C[0x20];
  D_800F4538_t2 unk16C;
  u_char unk178[0x434];
  u_int unk5AC_0 : 3;
  u_int unk5AC_3 : 1;
  u_int unk5AC_4 : 2;
  u_int unk5AC_6 : 2;
  u_int unk5AC_8 : 1;
  u_int unk5AC_9 : 2;
  u_int unk5AC_11 : 1;
  u_int unk5AC_12 : 1;
  u_int unk5AC_13 : 1;
  u_int unk5AC_14 : 1;
  u_int unk5AC_15 : 1;
  u_int unk5AC_16 : 2;
  u_int unk5AC_18 : 1;
  u_int unk5AC_19 : 1;
  u_int unk5AC_20 : 1;
  u_int unk5AC_21 : 1;
  u_int unk5AC_22 : 1;
  u_int unk5AC_23 : 1;
  u_int unk5AC_24 : 1;
  u_int unk5AC_25 : 7;
  u_char unk5B0;
  u_char unk5B1;
  u_char unk5B2;
  u_char unk5B3;
  u_char unk5B4;
  u_char unk5B5;
  u_char unk5B6;
  u_char unk5B7;
  u_char unk5B8;
  u_char unk5B9;
  u_char unk5BA;
  u_char unk5BB;
  short unk5BC;
  short unk5BE;
  u_char unk5C0;
  u_char unk5C1;
  u_short unk5C2;
  int unk5C4;
  int unk5C8;
  char unk5CC;
  char unk5CD;
  short unk5CE;
  int unk5D0;
  int unk5D4;
  void *unk5D8;
  int unk5DC;
  int unk5E0;
  int unk5E4;
  int unk5E8;
  u_char unk5EC[0x50];
  u_short unk63C;
  u_short unk63E;
  u_char unk640[8];
  int unk648;
  u_char unk64C;
  u_char unk64D;
  u_short unk64E;
  u_short unk650;
  u_short unk652;
  u_short unk654;
  u_short unk656;
  u_char unk658[0x88];
  u_char unk6E0;
  u_char unk6E1;
  u_char unk6E2;
  u_char unk6E3;
  u_char unk6E4;
  u_char unk6E5;
  u_short unk6E6;
  short unk6E8;
  short unk6EA;
  int unk6EC;
  u_char unk6F0;
  u_char unk6F1;
  u_char unk6F2;
  u_char unk6F3;
  D_800F4538_unk58 unk6F4;
  D_800F4538_unk58_2 unk6F8;
  int unk6FC;
  int unk700;
  D_800F4538_unkC54 unk704;
  D_800F4538_unkC54 unkC54;
  u_char unk11A4[0x280];
  u_short unk1424[0xA0];
  u_short unk1564[0xA0];
  u_char unk16A4[0x158];
  u_char unk17FC;
  u_char unk17FD;
  u_char unk17FE;
  u_char unk17FF;
  u_char unk1800[0x3C];
  short unk183C;
  short unk183E;
  short unk1840;
  short unk1842;
  short unk1844;
  u_char unk1846;
  u_char unk1847;
  u_char unk1848;
  u_char unk1849;
  u_char unk184A;
  u_char unk184B;
  u_short unk184C;
  u_short unk184E;
  u_int unk1850;
  u_char unk1854[0x10];
  D_800F4538_unk1864 unk1864;
  int unk1868;
  int unk187C;
} D_800F45E0_t;
typedef struct 
{
  int unk0;
  u_char excluded[17];
  u_char pad15[3];
  D_800F45E0_t *unk18;
  short unk1C[8];
  u_char blocked[8];
  u_char attributes[8];
  u_char centerAttribute;
  u_char pad3D[3];
  int unk40;
} D_1F8003BC_t;
int func_800A1108(int, void *);
void func_800A36E0(int, int, func_8006EBF8_t *);
int func_800A4494(int, SVECTOR *);
void func_800A48CC(int, int, int);
void func_800A4D8C(void);
void func_800A30A0(int, func_80089888_t *, int, int);
void func_800A35A8(void);
D_800F4538_t *func_800A3C00(D_800F45E0_t *arg0, u_int arg1);
void func_800A4828(int, MATRIX *);
void func_800A9D24(int, SVECTOR *, int);
int func_800A3760(int, int, int);
int func_800A38E0(int);
int func_800A3E6C(int);
int func_800A47C4(void);
void func_800A4E68(int);
extern D_800F4538_t *D_800F4538[17];
extern vs_battle_wepModels_t *vs_battle_wepModels[20];
extern D_800F45E0_t *D_800F45E0[16];
short func_800BEB10(void);
void func_800BEB34(void);
void func_800BEB9C(int);
void func_800BEBEC(void);
char vs_battle_getStateFlag(short);
void vs_battle_setStateFlag(short id, char value);
int func_800BEC30(void);
short func_800BEC58(int, int, u_char *, int);
int vs_battle_invalidOpcode(u_char *, short);
int func_800B66F4(u_char *, short);
int func_800B6724(u_char *, short);
int func_800B6744(u_char *, short);
int func_800B6868(u_char *, short);
int func_800B68C4(u_char *, short);
int func_800B66E4(u_char *, short);
int vs_battle_script_dismissTextBox(u_char *, short);
int func_800B66EC(u_char *, short);
int vs_battle_script_loadGim(u_char *, short);
int func_800B7014(u_char *, short);
int func_800B70E8(u_char *, short);
int func_800B71C4(u_char *, short);
int vs_battle_script_setStringContextInt(u_char *, short);
int func_800B7358(u_char *, short);
int func_800B7428(u_char *, short);
int func_800B7490(u_char *, short);
int func_800B7574(u_char *, short);
int func_800B75F4(u_char *, short);
int func_800B770C(u_char *, short);
int func_800B77DC(u_char *, short);
int func_800B784C(u_char *, short);
int func_800B792C(u_char *, short);
int func_800B7A98(u_char *, short);
int func_800B7B58(u_char *, short);
int func_800B7C90(u_char *, short);
int func_800B7D10(u_char *, short);
int func_800B7DC4(u_char *, short);
int func_800B7EF0(u_char *, short);
int func_800B7F84(u_char *, short);
int func_800B804C(u_char *, short);
int func_800B80B4(u_char *, short);
int func_800B81A0(u_char *, short);
int func_800B8290(u_char *, short);
int func_800B8360(u_char *, short);
int func_800B849C(u_char *, short);
int func_800B8510(u_char *, short);
int func_800B8590(u_char *, short);
int func_800B860C(u_char *, short);
int func_800B86E8(u_char *, short);
int func_800B8708(u_char *, short);
int func_800B8764(u_char *, short);
int func_800B8788(u_char *, short);
int func_800B884C(u_char *, short);
int vs_battle_script_jumpFwdIfFlag(u_char *, short);
int vs_battle_script_jumpFwdIfFlags(u_char *, short);
int vs_battle_script_jumpFwd(u_char *, short);
int vs_battle_script_jumpFwd2(u_char *, short);
int vs_battle_script_setFlagImmContext(u_char *, short);
int vs_battle_script_jumpFwdIfFlagContext(u_char *, short);
int func_800B8C54(u_char *, short);
int vs_battle_script_jumpBackIf(u_char *, short);
int vs_battle_script_jumpFwd4(u_char *, short);
int vs_battle_script_jumpFwd3(u_char *, short);
int vs_battle_script_setFlagImm(u_char *, short);
int vs_battle_script_setFlag(u_char *, short);
int vs_battle_script_setFlagIfItemInInventory(u_char *, short);
int func_800B91C4(u_char *, short);
int func_800B9230(u_char *, short);
int vs_battle_script_setFlagIfButton(u_char *, short);
int func_800B92A0(u_char *, short);
int func_800B92D0(u_char *, short);
int func_800B92F4(u_char *, short);
int func_800B94D8(u_char *, short);
int func_800B9564(u_char *, short);
int func_800B9608(u_char *, short);
int func_800B9670(u_char *, short);
int func_800B9724(u_char *, short);
int func_800B9770(u_char *, short);
int func_800B979C(u_char *, short);
int func_800B97E4(u_char *, short);
int func_800B97F8(u_char *, short);
int func_800B9818(u_char *, short);
int func_800B983C(u_char *, short);
int func_800B9860(u_char *, short);
int func_800B988C(u_char *, short);
int func_800B98C0(u_char *, short);
int func_800B9AC4(u_char *, short);
int func_800B9AE8(u_char *, short);
int func_800B9C18(u_char *, short);
int func_800B9C58(u_char *, short);
int func_800B9D74(u_char *, short);
int func_800B9DB4(u_char *, short);
int func_800B9F28(u_char *, short);
int func_800B9F4C(u_char *, short);
int func_800B9F70(u_char *, short);
int func_800B9F90(u_char *, short);
int func_800B9FC0(u_char *, short);
int func_800BA0E4(u_char *, short);
int func_800BA194(u_char *, short);
int func_800BA108(u_char *, short);
int func_800BA218(u_char *, short);
int func_800BA264(u_char *, short);
int func_800BA288(u_char *, short);
int func_800BA2B4(u_char *, short);
int func_800BA3E4(u_char *, short);
int func_800BA404(u_char *, short);
int vs_battle_script_loadSfxSlot(u_char *, short);
int vs_battle_script_freeSfxSlot(u_char *, short);
int vs_battle_script_freeSfx(u_char *, short);
int vs_battle_script_setCurrentSfx(u_char *, short);
int vs_battle_script_loadSoundFile(u_char *, short);
int vs_battle_script_loadMusicSlot(u_char *, short);
int vs_battle_script_freeMusic(u_char *, short);
int vs_battle_script_clearMusicLoadSlot(u_char *, short);
int vs_battle_script_loadSoundFile2(u_char *, short);
int vs_battle_script_processSoundQueue(u_char *, short);
int func_800BA8BC(u_char *, short);
int func_800BA90C(u_char *, short);
int func_800BA94C(u_char *, short);
int func_800BAA04(u_char *, short);
int func_800BAA34(u_char *, short);
int func_800BAA58(u_char *, short);
int func_800BAACC(u_char *, short);
int func_800BAAF0(u_char *, short);
int func_800BABCC(u_char *, short);
int func_800BAC38(u_char *, short);
int func_800BAC5C(u_char *, short);
int func_800BAD78(u_char *, short);
int func_800BAE74(u_char *, short);
int func_800BAEA0(u_char *, short);
int vs_battle_script_specialOp(u_char *, short);
int func_800BB288(u_char *, short);
int func_800BD6C4(u_char *, short);
int func_800BB2E4(u_char *, short);
int func_800BB304(u_char *, short);
int func_800BB30C(u_char *, short);
int func_800BC2E8(u_char *, short);
int func_800BB33C(u_char *, short);
int func_800BB35C(u_char *, short);
int func_800BB450(u_char *, short);
int func_800BD444(u_char *, short);
int func_800BB38C(u_char *, short);
int func_800BB3BC(u_char *, short);
int func_800BDC9C(u_char *, short);
int func_800BB4CC(u_char *, short);
int vs_battle_script_playMenuSelectSfx(u_char *, short);
int func_800BB4F0(u_char *, short);
int func_800BB504(u_char *, short);
int func_800BB554(u_char *, short);
int func_800BB5A4(u_char *, short);
int func_800BB5AC(u_char *, short);
int func_800BB604(u_char *, short);
int _loadMpdClearedScriptDoorSections(void *clearedSection, u_short clearedSectionLen, void *scriptSection, u_short scriptSectionLen, void *doorSection, u_short doorSectionLen, int startState);
void func_800B64A8(int, int, int);
int func_800BF850(void);
void func_800B63AC(int, int *, int, int);
void func_800B6404(int, int *, int, int);
void func_800B6438(int, int *, int, int);
void func_800B64CC(int, int, int, int);
void func_800B6500(int, SVECTOR *);
void func_800B6530(int, SVECTOR *, int);
void func_800B653C(int, SVECTOR *, int, int);
void func_800B6580(int, int *, int);
void func_800B65E8(int, int *, int);
void func_800B65F4(int, int *, int);
void func_800B6634(int, int);
void func_800B668C(int arg0, short arg1, int arg2);
extern short D_800F4BE2;
extern char vs_battle_textBoxStatuses[];
extern char D_800F4C08;
extern char D_800F4C20;
extern char D_800F4C2C;
extern void *D_800F4C30;
extern void *D_800F4C48;
extern short D_800F4C58[];
extern char D_800F4C68;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef struct _physadr
{
  int r[1];
} *physadr;
typedef struct label_t
{
  int val[12];
} label_t;
typedef struct _quad
{
  long val[2];
} quad;
typedef long daddr_t;
typedef char *caddr_t;
typedef long *qaddr_t;
typedef u_long ino_t;
typedef long swblk_t;
typedef long time_t;
typedef short dev_t;
typedef long off_t;
typedef u_short uid_t;
typedef u_short gid_t;
typedef struct 
{
  int unk0;
  int unk4;
  char unk8;
  char unk9;
} vs_main_sfxContext;
int TeardownSound(void);
int InitSound(void);
int Sound_BindAkaoSfxBlob(void *arg0);
int func_80011EBC(u_int);
int func_80011F08(void);
int func_80011F90(void);
u_int func_80011FB4(void *);
void func_80011FDC(u_int);
void func_80012004(void);
int func_80012024(void);
u_int func_80012080(void *, u_int, int);
u_int func_800120E8(void *);
int func_8001217C(u_long, int, int, int);
int func_800121F0(void *, int, int, int);
void func_80012288(u_long, int);
int func_80012358(int);
int func_800123C8(vs_main_sfxContext *);
void vs_sound_setMonoSound(int);
void func_80012468(void *);
void func_8001264C(int, int, int, int);
void func_80012738(int, int, int, int);
void func_80012824(int, int, int, int);
void func_800128A0(u_long, int, int);
void func_80012918(int);
void func_80012940(int, int);
void func_800129D0(int, int);
int func_80012A6C(int, int);
void func_80012B78(void);
void func_80012B98(void);
void func_80012BB8(void *, int);
int vs_sound_spuTransferring(void);
int func_80012EBC(void);
int func_80013188(void *, int);
int func_800131DC(void *, int, int);
int vs_sound_setCdVol(u_int);
void func_800132C4(void *, int, int);
void func_80013328(void);
void func_80013378(int, int);
typedef struct 
{
  char range;
  char unk1;
  char unk2;
  char attackShape : 3;
  char attackShapeAngle : 5;
} vs_battle_range_t;
typedef struct 
{
  u_char id;
  char subId;
  char wepId;
  char category;
  u_short maxDp;
  u_short maxPp;
  u_short currentDp;
  u_short currentPp;
  signed char strength;
  signed char intelligence;
  signed char agility;
  char cost;
  char damageType : 2;
  char costType : 3;
  char unk10_5 : 3;
  char unk11;
  u_short unk12;
  vs_battle_range_t range;
  signed char classes[8];
  signed char affinities[8];
  u_short material;
  char assembledWeaponIndex;
  char index;
} vs_main_inventoryBlade;
typedef struct 
{
  u_short id;
  char subId;
  u_char category;
  char gemSlots;
  signed char strength;
  signed char intelligence;
  signed char agility;
  signed char types[4];
  u_short assembledWeaponIndex;
  u_short index;
} vs_main_inventoryGrip;
typedef struct 
{
  u_short id;
  char subId;
  char unk3;
  char gemEffects;
  signed char strength;
  signed char intelligence;
  signed char agility;
  signed char classes[8];
  signed char affinities[8];
  u_short setItemIndex;
  short index;
} vs_main_inventoryGem;
typedef struct 
{
  char index;
  u_char blade;
  u_char grip;
  char isEquipped;
  u_char gems[4];
  char name[24];
} vs_main_inventoryWeapon;
typedef struct 
{
  u_char id;
  u_char subId;
  char wepId;
  char category;
  u_short maxDp;
  u_short maxPp;
  u_short currentDp;
  u_short currentPp;
  char gemSlots;
  signed char strength;
  signed char intelligence;
  signed char agility;
  signed char types[4];
  signed char classes[8];
  signed char affinities[8];
  u_short material;
  u_char limb;
  u_char index;
} vs_main_inventoryArmor;
typedef struct 
{
  char index;
  char isEquipped;
  short unk2;
  vs_main_inventoryArmor base;
  u_char gems[4];
} vs_main_inventoryShield;
typedef struct 
{
  u_short id;
  char count;
  char index;
} vs_main_inventoryMisc;
typedef struct 
{
  vs_main_inventoryWeapon weapons[32];
  vs_main_inventoryShield shields[32];
  vs_main_inventoryBlade blades[64];
  vs_main_inventoryGrip grips[64];
  vs_main_inventoryArmor armor[64];
  vs_main_inventoryGem gems[192];
  vs_main_inventoryMisc misc[256];
} vs_menu_containerData;
typedef struct 
{
  u_short weapons[32];
  u_short blades[64];
  u_short grips[64];
  u_short shields[32];
  u_short armor[64];
  u_short gems[192];
  u_short misc[256];
} vs_menu_containerIndices;
typedef struct 
{
  vs_menu_containerData data;
  vs_menu_containerIndices indices;
} vs_menu_container;
typedef struct 
{
  u_char weapons[8];
  u_char blades[16];
  u_char grips[16];
  u_char shields[8];
  u_char armor[16];
  u_char gems[48];
  u_char misc[64];
} vs_main_inventoryIndices_t;
typedef struct 
{
  char unk0[15][256];
  vs_main_inventoryIndices_t unkF00;
  vs_menu_containerData unkFB0;
  vs_menu_containerData unk4BB0;
  vs_menu_containerData unk87B0;
  u_short itemsToTransfer[64];
  vs_menu_container unkC430;
  vs_menu_container unk105B0;
} vs_menu_inventoryStorage_t;
enum vs_itemCategories
{
  itemCategoryWeapon,
  itemCategoryBlade,
  itemCategoryGrip,
  itemCategoryShield,
  itemCategoryArmor,
  itemCategoryGem,
  itemCategoryMisc,
  itemCategoryManage
};
enum vs_gemTargets
{
  gemTargetShield = 0x80
};
typedef struct 
{
  char difficultyFlags;
  char unk1;
  char unk2;
  char unk3;
  vs_main_inventoryBlade blade;
  vs_main_inventoryGrip grip;
  vs_main_inventoryGem gems[3];
  char name[24];
} vs_battle_lootedWeapon;
typedef struct 
{
  char difficultyFlags;
  char unk1;
  char unk2;
  char unk3;
  vs_main_inventoryBlade blade;
} vs_battle_lootedBlade;
typedef struct 
{
  char difficultyFlags;
  char unk1;
  char unk2;
  char unk3;
  vs_main_inventoryGrip grip;
} vs_battle_lootedGrip;
typedef struct 
{
  char difficultyFlags;
  char unk1;
  char unk2;
  char unk3;
  vs_main_inventoryArmor shield;
  vs_main_inventoryGem gems[3];
} vs_battle_lootedShield;
typedef struct 
{
  char difficultyFlags;
  char unk1;
  char unk2;
  char unk3;
  vs_main_inventoryArmor armor;
} vs_battle_lootedArmor;
typedef struct 
{
  char difficultyFlags;
  char unk1;
  char unk2;
  char unk3;
  vs_main_inventoryArmor accessory;
} vs_battle_lootedAccessory;
typedef struct 
{
  char difficultyFlags;
  char unk1;
  char unk2;
  char unk3;
  vs_main_inventoryGem gem;
} vs_battle_lootedGem;
typedef struct 
{
  u_short id;
  char difficultyFlags;
  char count;
} vs_battle_lootedMisc;
typedef struct 
{
  vs_battle_lootedWeapon weapon;
  vs_battle_lootedBlade blade;
  vs_battle_lootedGrip grip;
  vs_battle_lootedShield shield;
  vs_battle_lootedArmor armor[2];
  vs_battle_lootedAccessory accessory;
  vs_battle_lootedGem gem;
  vs_battle_lootedMisc misc[3];
} vs_battle_loot;
extern int (*GPU_printf)(char *fmt, ...);
typedef struct 
{
  short x;
  short y;
  short w;
  short h;
} RECT;
typedef struct 
{
  int x;
  int y;
  int w;
  int h;
} RECT32;
typedef struct 
{
  u_long tag;
  u_long code[15];
} DR_ENV;
typedef struct 
{
  RECT clip;
  short ofs[2];
  RECT tw;
  u_short tpage;
  u_char dtd;
  u_char dfe;
  u_char isbg;
  u_char r0;
  u_char g0;
  u_char b0;
  DR_ENV dr_env;
} DRAWENV;
typedef struct 
{
  RECT disp;
  RECT screen;
  u_char isinter;
  u_char isrgb24;
  u_char pad0;
  u_char pad1;
} DISPENV;
typedef struct 
{
  unsigned addr : 24;
  unsigned len : 8;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
} P_TAG;
typedef struct 
{
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
} P_CODE;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  short x1;
  short y1;
  short x2;
  short y2;
} POLY_F3;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  short x1;
  short y1;
  short x2;
  short y2;
  short x3;
  short y3;
} POLY_F4;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char u0;
  u_char v0;
  u_short clut;
  short x1;
  short y1;
  u_char u1;
  u_char v1;
  u_short tpage;
  short x2;
  short y2;
  u_char u2;
  u_char v2;
  u_short pad1;
} POLY_FT3;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char u0;
  u_char v0;
  u_short clut;
  short x1;
  short y1;
  u_char u1;
  u_char v1;
  u_short tpage;
  short x2;
  short y2;
  u_char u2;
  u_char v2;
  u_short pad1;
  short x3;
  short y3;
  u_char u3;
  u_char v3;
  u_short pad2;
} POLY_FT4;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char r1;
  u_char g1;
  u_char b1;
  u_char pad1;
  short x1;
  short y1;
  u_char r2;
  u_char g2;
  u_char b2;
  u_char pad2;
  short x2;
  short y2;
} POLY_G3;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char r1;
  u_char g1;
  u_char b1;
  u_char pad1;
  short x1;
  short y1;
  u_char r2;
  u_char g2;
  u_char b2;
  u_char pad2;
  short x2;
  short y2;
  u_char r3;
  u_char g3;
  u_char b3;
  u_char pad3;
  short x3;
  short y3;
} POLY_G4;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char u0;
  u_char v0;
  u_short clut;
  u_char r1;
  u_char g1;
  u_char b1;
  u_char p1;
  short x1;
  short y1;
  u_char u1;
  u_char v1;
  u_short tpage;
  u_char r2;
  u_char g2;
  u_char b2;
  u_char p2;
  short x2;
  short y2;
  u_char u2;
  u_char v2;
  u_short pad2;
} POLY_GT3;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char u0;
  u_char v0;
  u_short clut;
  u_char r1;
  u_char g1;
  u_char b1;
  u_char p1;
  short x1;
  short y1;
  u_char u1;
  u_char v1;
  u_short tpage;
  u_char r2;
  u_char g2;
  u_char b2;
  u_char p2;
  short x2;
  short y2;
  u_char u2;
  u_char v2;
  u_short pad2;
  u_char r3;
  u_char g3;
  u_char b3;
  u_char p3;
  short x3;
  short y3;
  u_char u3;
  u_char v3;
  u_short pad3;
} POLY_GT4;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  short x1;
  short y1;
} LINE_F2;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char r1;
  u_char g1;
  u_char b1;
  u_char p1;
  short x1;
  short y1;
} LINE_G2;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  short x1;
  short y1;
  short x2;
  short y2;
  u_long pad;
} LINE_F3;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char r1;
  u_char g1;
  u_char b1;
  u_char p1;
  short x1;
  short y1;
  u_char r2;
  u_char g2;
  u_char b2;
  u_char p2;
  short x2;
  short y2;
  u_long pad;
} LINE_G3;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  short x1;
  short y1;
  short x2;
  short y2;
  short x3;
  short y3;
  u_long pad;
} LINE_F4;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char r1;
  u_char g1;
  u_char b1;
  u_char p1;
  short x1;
  short y1;
  u_char r2;
  u_char g2;
  u_char b2;
  u_char p2;
  short x2;
  short y2;
  u_char r3;
  u_char g3;
  u_char b3;
  u_char p3;
  short x3;
  short y3;
  u_long pad;
} LINE_G4;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char u0;
  u_char v0;
  u_short clut;
  short w;
  short h;
} SPRT;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char u0;
  u_char v0;
  u_short clut;
} SPRT_16;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  u_char u0;
  u_char v0;
  u_short clut;
} SPRT_8;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
  short w;
  short h;
} TILE;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
} TILE_16;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
} TILE_8;
typedef struct 
{
  u_long tag;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char code;
  short x0;
  short y0;
} TILE_1;
typedef struct 
{
  u_long tag;
  u_long code[2];
} DR_MODE;
typedef struct 
{
  u_long tag;
  u_long code[2];
} DR_TWIN;
typedef struct 
{
  u_long tag;
  u_long code[2];
} DR_AREA;
typedef struct 
{
  u_long tag;
  u_long code[2];
} DR_OFFSET;
typedef struct 
{
  u_long tag;
  u_long code[5];
} DR_MOVE;
typedef struct 
{
  u_long tag;
  u_long code[3];
  u_long p[13];
} DR_LOAD;
typedef struct 
{
  u_long tag;
  u_long code[1];
} DR_TPAGE;
typedef struct 
{
  u_long tag;
  u_long code[2];
} DR_STP;
typedef struct 
{
  u_long id;
  u_char r0;
  u_char g0;
  u_char b0;
  u_char p0;
  u_char r1;
  u_char g1;
  u_char b1;
  u_char p1;
  u_char r2;
  u_char g2;
  u_char b2;
  u_char p2;
  u_char r3;
  u_char g3;
  u_char b3;
  u_char p3;
  u_short tpage;
  u_short clut;
  u_char u0;
  u_char v0;
  u_char u1;
  u_char v1;
  u_char u2;
  u_char v2;
  u_char u3;
  u_char v3;
  SVECTOR x0;
  SVECTOR x1;
  SVECTOR x2;
  SVECTOR x3;
  SVECTOR n0;
  SVECTOR n1;
  SVECTOR n2;
  SVECTOR n3;
  SVECTOR *v_ofs;
  SVECTOR *n_ofs;
  u_short vert0;
  u_short vert1;
  u_short vert2;
  u_short vert3;
  u_short norm0;
  u_short norm1;
  u_short norm2;
  u_short norm3;
} TMD_PRIM;
typedef struct 
{
  u_long mode;
  RECT *crect;
  u_long *caddr;
  RECT *prect;
  u_long *paddr;
} TIM_IMAGE;
extern int FntPrint();
extern int KanjiFntPrint();
extern DISPENV *GetDispEnv(DISPENV *env);
extern DISPENV *PutDispEnv(DISPENV *env);
extern DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
extern DRAWENV *GetDrawEnv(DRAWENV *env);
extern DRAWENV *PutDrawEnv(DRAWENV *env);
extern DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
extern TIM_IMAGE *ReadTIM(TIM_IMAGE *timimg);
extern TMD_PRIM *ReadTMD(TMD_PRIM *tmdprim);
extern int CheckPrim(char *s, u_long *p);
extern int ClearImage(RECT *rect, u_char r, u_char g, u_char b);
extern int ClearImage2(RECT *rect, u_char r, u_char g, u_char b);
extern int DrawSync(int mode);
extern int FntOpen(int x, int y, int w, int h, int isbg, int n);
extern int GetGraphDebug(void);
extern int GetTimSize(u_char *sjis);
extern int IsEndPrim(void *p);
extern int KanjiFntOpen(int x, int y, int w, int h, int dx, int dy, int cx, int cy, int isbg, int n);
extern void KanjiFntClose(void);
extern int Krom2Tim(u_char *sjis, u_long *taddr, int dx, int dy, int cdx, int cdy, u_int fg, u_int bg);
extern int LoadImage(RECT *rect, u_long *p);
extern int MargePrim(void *p0, void *p1);
extern int MoveImage(RECT *rect, int x, int y);
extern int OpenTIM(u_long *addr);
extern int OpenTMD(u_long *tmd, int obj_no);
extern int ResetGraph(int mode);
extern int SetGraphDebug(int level);
extern int StoreImage(RECT *rect, u_long *p);
extern u_long *ClearOTag(u_long *ot, int n);
extern u_long *ClearOTagR(u_long *ot, int n);
extern u_long *FntFlush(int id);
extern u_long *KanjiFntFlush(int id);
extern u_long DrawSyncCallback(void (*func)(void));
extern u_short GetClut(int x, int y);
extern u_short GetTPage(int tp, int abr, int x, int y);
extern u_short LoadClut(u_long *clut, int x, int y);
extern u_short LoadClut2(u_long *clut, int x, int y);
extern u_short LoadTPage(u_long *pix, int tp, int abr, int x, int y, int w, int h);
extern void *NextPrim(void *p);
extern void AddPrim(void *ot, void *p);
extern void AddPrims(void *ot, void *p0, void *p1);
extern void CatPrim(void *p0, void *p1);
extern void DrawOTag(u_long *p);
extern void DrawOTagIO(u_long *p);
extern void DrawOTagEnv(u_long *p, DRAWENV *env);
extern void DrawPrim(void *p);
extern void DumpClut(u_short clut);
extern void DumpDispEnv(DISPENV *env);
extern void DumpDrawEnv(DRAWENV *env);
extern void DumpOTag(u_long *p);
extern void DumpTPage(u_short tpage);
extern void FntLoad(int tx, int ty);
extern void SetDispMask(int mask);
extern void SetDrawArea(DR_AREA *p, RECT *r);
extern void SetDrawEnv(DR_ENV *dr_env, DRAWENV *env);
extern void SetDrawLoad(DR_LOAD *p, RECT *rect);
extern void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
extern void SetDrawTPage(DR_TPAGE *p, int dfe, int dtd, int tpage);
extern void SetDrawMove(DR_MOVE *p, RECT *rect, int x, int y);
extern void SetDrawOffset(DR_OFFSET *p, u_short *ofs);
extern void SetDrawStp(DR_STP *p, int pbw);
extern void SetDumpFnt(int id);
extern void SetLineF2(LINE_F2 *p);
extern void SetLineF3(LINE_F3 *p);
extern void SetLineF4(LINE_F4 *p);
extern void SetLineG2(LINE_G2 *p);
extern void SetLineG3(LINE_G3 *p);
extern void SetLineG4(LINE_G4 *p);
extern void SetPolyF3(POLY_F3 *p);
extern void SetPolyF4(POLY_F4 *p);
extern void SetPolyFT3(POLY_FT3 *p);
extern void SetPolyFT4(POLY_FT4 *p);
extern void SetPolyG3(POLY_G3 *p);
extern void SetPolyG4(POLY_G4 *p);
extern void SetPolyGT3(POLY_GT3 *p);
extern void SetPolyGT4(POLY_GT4 *p);
extern void SetSemiTrans(void *p, int abe);
extern void SetShadeTex(void *p, int tge);
extern void SetSprt(SPRT *p);
extern void SetSprt16(SPRT_16 *p);
extern void SetSprt8(SPRT_8 *p);
extern void SetTexWindow(DR_TWIN *p, RECT *tw);
extern void SetTile(TILE *p);
extern void SetTile1(TILE_1 *p);
extern void SetTile16(TILE_16 *p);
extern void SetTile8(TILE_8 *p);
extern void TermPrim(void *p);
extern u_long *BreakDraw(void);
extern void ContinueDraw(u_long *insaddr, u_long *contaddr);
extern int IsIdleGPU(int max_count);
extern int GetODE(void);
extern int LoadImage2(RECT *rect, u_long *p);
extern int StoreImage2(RECT *rect, u_long *p);
extern int MoveImage2(RECT *rect, int x, int y);
extern int DrawOTag2(u_long *p);
extern void GetDrawMode(DR_MODE *p);
extern void GetTexWindow(DR_TWIN *p);
extern void GetDrawArea(DR_AREA *p);
extern void GetDrawOffset(DR_OFFSET *p);
extern void GetDrawEnv2(DR_ENV *p);
typedef struct 
{
  char mode;
  char exId;
  char states;
  char rStickX;
  char rStickY;
  char lStickX;
  char lStickY;
  char lock;
  char actData[2];
  char connected;
} portInfo_t;
typedef struct 
{
  int lba;
  int size;
} vs_main_CdFile;
enum vs_main_CdQueueState
{
  vs_main_CdQueueStateFree = 0,
  vs_main_CdQueueStateAllocated = 1,
  vs_main_CdQueueStateReading = 2,
  vs_main_CdQueueStateEnqueued = 3,
  vs_main_CdQueueStateLoaded = 4,
  vs_main_CdQueueStateError = 6,
  vs_main_CdQueueStateDispatched = 7
};
typedef struct 
{
  short state;
  short priority;
  vs_main_CdFile cdFile;
  int unkC;
  void *vram;
} vs_main_CdQueueSlot;
enum actions_e
{
  vs_main_actions_daggerArt1 = 184
};
typedef union 
{
  u_short u16[2];
  u_int s32;
} vs_hitParams_t;
enum actionTypes
{
  actionTypeSpell = 1,
  actionTypeAbility,
  actionTypeBreakArt,
  actionTypeItem = 5,
  actionTypeBaseAttack,
  actionTypeTrap
};
enum enemyClasses
{
  enemyClassUndead = 2
};
typedef struct 
{
  u_short unk0 : 4;
  u_short unk4 : 1;
  u_short unk5 : 1;
  u_short unk6 : 1;
  u_short unk7 : 1;
  u_short unk8 : 4;
  u_short unk12 : 1;
  u_short unk13 : 1;
  u_short unk14 : 2;
} vs_actionFlags_t;
typedef struct 
{
  u_char id;
  u_char unk1;
  u_char unk2_0 : 1;
  u_char type : 3;
  u_char unk2_4 : 4;
  u_char cost;
  u_char rangeX;
  u_char rangeY;
  u_char rangeZ;
  u_char shape : 3;
  u_char angle : 5;
  u_char aoe_0;
  u_char aoe_8;
  u_char aoe_16;
  u_char aoe_24;
  u_char flags_0;
  u_int flagsD_0 : 4;
  u_int flagsD_4 : 2;
  u_int flagsD_6 : 1;
  u_int unlocked : 1;
  vs_actionFlags_t flagsE;
  int unk10;
  struct 
  {
    u_int effect : 7;
    u_int prerequisiteFunction : 6;
    u_int hitFunction : 3;
    u_int statCalculator : 6;
    u_int statFactor : 5;
    u_int type : 2;
    u_int affinity : 3;
  } hitParams[2];
  char name[24];
} vs_action_t;
typedef struct 
{
  int unk0;
  u_short unk4;
  u_short unk6;
  int unk8;
  int unkC;
} vs_main_statusEffectParams_t;
typedef struct 
{
  int currentMusicSlot;
  u_int unk4;
  u_int unk8;
  void *currentMusicData;
  char musicIds[4];
  u_int unk14[4];
  void *musicData[4];
  vs_main_CdQueueSlot *musicQueueSlot[4];
  u_int currentSfxId;
  char sfxIds[4];
  void *sfxData[3];
  vs_main_CdQueueSlot *sfxQueueSlot[3];
  void *unk64[3];
  int *unk70[3];
  int soundFileId;
  void *soundData;
  vs_main_CdQueueSlot *soundQueueSlot;
} soundData_t;
typedef struct vs_main_HeapHeader
{
  struct vs_main_HeapHeader *prev;
  struct vs_main_HeapHeader *next;
  u_int blockSz;
  int unkC;
} vs_main_HeapHeader;
typedef struct 
{
  u_char unk0_0 : 4;
  u_char containerEmpty : 1;
  u_char abilityTimingDisplay : 1;
  u_char weaponStatChange : 1;
  u_char armorStatChange : 1;
  char cursorMemory;
  u_short menuFlags;
  u_int slotState;
  char information;
  char simpleMap;
  char monoSound;
  char vibrationOn;
  char mappedChainAbilities[4];
  char mappedDefenseAbilities[4];
  u_int key;
  u_short cumulativeSaveCount;
  u_short currentGameSaveCount;
  u_int saveFileGeneration;
} vs_main_settings_t;
typedef union 
{
  struct 
  {
    signed char f;
    signed char s;
    signed char m;
    signed char h;
  } t;
  u_int all;
} vs_Gametime_t;
typedef struct 
{
  u_int time : 24;
  u_int round : 7;
  u_int difficulty : 1;
} vs_trialScore_t;
typedef struct 
{
  int titles;
  u_short enemyKills[6];
  int unk10;
  u_short weaponAttacks[10];
  vs_trialScore_t bossTimeTrialScores[8][3];
  u_short maxChain;
  u_short unk8A;
  u_int clearCount;
  u_int completionTimeMinutes;
  u_int mapCompletion;
  u_int openedChestCount;
  int enemyKillFlags[26];
  u_int streakScore;
  u_int miscScore;
  char killStreakEnemyClass;
  char killStreakEnemyCount;
  char weaponKillStreakBladeCategory;
  char weaponKillStreak;
  u_short chainStreak;
  u_short healCount;
} vs_main_scoredata_t;
typedef struct 
{
  u_short hp;
  u_short maxHp;
  u_short types[4];
  u_short affinities[8];
} D_80060068_t2;
typedef struct 
{
  int unk0;
  short currentHP;
  short maxHP;
  short currentMP;
  short maxMP;
  short strength;
  short totalStrength;
  short intelligence;
  short totalIntelligence;
  short agility;
  short totalAgility;
  short risk;
  u_short flags;
  int statuses;
  char statusTimers[8];
  D_80060068_t2 limbs[6];
  int weaponDrawn;
  int unk80;
  int unk84;
} D_80060068_t3;
typedef struct 
{
  D_80060068_t3 unk0;
  int unkDC[9];
} D_80060068_t;
enum itemIdentifiers
{
  bronzeKey = 458,
  chamomileSigil = 466
};
typedef struct 
{
  char clearCount;
  char difficulty;
  char normalModeCleared;
  char unk3;
  char unk4;
  char unk5;
  char unk6;
  char unk7;
  char unk8;
  char unk9;
  char unkA;
  char unkB;
  char unkC;
  char unkD;
  char unkE;
  char unkF;
  char textBoxSelection;
  char unk11;
  char unk12;
  char unk13;
  char unk14;
  char unk15;
  char unk16;
  char unk17;
  char unk18;
  char unk19;
  char unk1A;
  char unk1B;
  char unk1C;
  char unk1D;
  char unk1E;
  char unk1F;
  char unk20;
  char unk21[0x1F];
  char unk40;
  char unk41[0x5F];
  char puzzleTimerHundredths;
  char puzzleTimerSeconds;
  char puzzleTimerMins;
  char puzzleSetMaxTimer;
  char gameOver;
  char unkA5;
  char unkA6;
  char unkA7;
  char unkA8;
  char unkA9;
  char unkAA;
  char introState;
  char unkAC[9];
  char unkB5;
  char menuFSubmenu;
  char magicUsed;
  char battleAbilitiesUsed;
  char breakArtsUsed;
  char unkBA;
  char unkBB;
  char unkBC;
  char unkBD;
  char unkBE;
  char unkBF;
  char unkC0;
  char unkC1;
  char unkC2;
  char unkC3;
  u_char timeAttackBoss;
  u_char timeAttackMins;
  u_char timeAttackSecs;
  u_char timeAttackHundredths;
  char unkC8;
  char unkC9;
  char unkCA;
  char unkCB;
  char damascusGolemDefeated;
  char damascusCrabDefeated;
  char ravanaDefeated;
  char dragonZombieDefeated;
  char deathAndOgreZombieDefeated;
  char asuraDefeated;
  char unkD2[51];
  char doorEntered;
  char unk106[22];
  char puzzleMode;
  char puzzleModeDisabled;
  char unk11E[23];
  char mapPaling[32];
  char unk135;
  char savePointStates[48];
  char unk186[48];
  char unk1B6[393];
  char unk33F[65];
  char bronzeKeyObtained;
  char ironKeyObtained;
  char silverKeyObtained;
  char goldKeyObtained;
  char platinumKeyObtained;
  char steelKeyObtained;
  char crimsonKeyObtained;
  char chestKeyObtained;
  char chamomileSigilUsed;
  char lilySigilUsed;
  char tearoseSigilUsed;
  char clematisSigilUsed;
  char hyacinthSigilUsed;
  char fernSigilUsed;
  char asterSigilUsed;
  char euleliaSigilUsed;
  char melissaSigilUsed;
  char callaSigilUsed;
  char laurelSigilUsed;
  char acaciaSigilUsed;
  char palmSigilUsed;
  char kalmiaSigilUsed;
  char colombineSigilUsed;
  char anemoneSigilUsed;
  char verbenaSigilUsed;
  char schirraSigilUsed;
  char marigoldSigilUsed;
  char azaleaSigilUsed;
  char tigertailSigilUsed;
  char stockSigilUsed;
  char cattleyaSigilUsed;
  char mandrakeSigilUsed;
  char unk3A8[32];
  char chestsOpened[64];
  char unk400[64];
} vs_main_flags_t;
typedef struct 
{
  u_char artsLearned[12];
  struct 
  {
    u_short weaponCategories[10];
    u_short battleAbilitiesUnlocked;
    u_short total;
  } kills;
} vs_main_artsStatus_t;
typedef struct 
{
  char r;
  char g;
  char b;
  char unk3;
} D_8005DC6C_t;
typedef struct 
{
  int lStickX;
  int lStickY;
  int rStickX;
  int rStickY;
} vs_main_stickPos;
typedef struct 
{
  int roomFlags[16];
  int areaFlags[2];
} vs_main_mapStatus_t;
typedef struct 
{
  vs_main_inventoryWeapon weapons[8];
  vs_main_inventoryShield shields[8];
  vs_main_inventoryBlade blades[16];
  vs_main_inventoryGrip grips[16];
  vs_main_inventoryArmor armor[16];
  vs_main_inventoryGem gems[48];
  vs_main_inventoryMisc misc[64];
} vs_main_inventory_t;
typedef struct 
{
  char unk0;
  char zndId;
  u_short mapId;
  u_int unk4;
  char unk8[8][0x28];
} D_80061078_t;
typedef struct 
{
  char unk0;
  char unk1;
  u_short unk2;
  signed char frames;
  signed char seconds;
  signed char minutes;
  signed char hours;
} D_80050468_t;
typedef struct 
{
  u_short unk0_0 : 5;
  u_short unk0_5 : 5;
  u_short unk0_10 : 3;
  u_short unk0_13 : 3;
} _mpdRoomDoorSection2;
typedef struct 
{
  u_char zndId;
  u_char mpdId;
  _mpdRoomDoorSection2 unk2;
  u_short unk4_0 : 8;
  u_short unk4_8 : 5;
  u_short unk4_13 : 2;
  u_short unk4_15 : 1;
  u_short unk6_0 : 10;
  u_short unk6_10 : 5;
  u_short unk6_15 : 1;
  u_char unk8;
  u_char unk9;
  u_char unkA;
  u_char unkB;
} _mpdRoomDoorSection_t;
extern portInfo_t _portInfo[2];
extern soundData_t vs_main_soundData;
extern vs_action_t vs_main_actions[256];
extern int D_8004A52C;
extern char vs_main_materialModifiers[8][30];
extern int vs_main_frameBuf;
extern DRAWENV vs_main_drawEnv[2];
extern DISPENV vs_main_dispEnv[2];
extern int vs_main_titleScreenCount;
extern vs_main_CdFile vs_main_zndFiles[256];
extern char vs_main_locationZoneMapIds[];
extern vs_main_statusEffectParams_t vs_main_statusEffectParams[];
extern char D_8004FCCC[];
extern char D_8004FDD0[];
extern u_short vs_main_artsPointsRequirements[][4];
extern u_short vs_main_battleAbilitiesPointsRequirements[];
extern D_80050468_t D_80050468;
extern void *D_80055C80[];
extern short D_8005DFD4;
extern short D_8005DFD6;
extern short D_8005DFD8;
extern u_short D_8005DFDA;
extern u_int D_8005E040;
extern void *D_8005E0C0[];
extern int vs_main_nearClip;
extern int vs_main_projectionDistance;
extern int vs_gametime_tickspeed;
extern int D_8005E1C4;
extern int D_8005E1D4;
extern int D_8005E244;
extern char vs_main_padBuffer[2][34];
extern union 
{
  u_int all;
  struct 
  {
    char low;
    char hi;
  } pad[2];
} vs_main_buttonsPressed;
extern int vs_main_buttonsPreviousState;
extern int vs_main_buttonRepeat;
extern int vs_main_buttonsReleased;
extern int vs_main_buttonsState;
extern int vs_main_saveGameClearData;
extern u_int vs_main_frameDuration;
extern vs_main_scoredata_t vs_main_scoredata;
extern char vs_main_actionsLearned[32];
extern vs_main_mapStatus_t vs_main_mapStatus;
extern vs_main_settings_t vs_main_settings;
extern int vs_main_startState;
extern vs_main_artsStatus_t vs_main_artsStatus;
extern int D_80060064;
extern D_80060068_t D_80060068;
extern vs_main_inventory_t vs_main_inventory;
extern _mpdRoomDoorSection_t D_80061068;
extern vs_Gametime_t vs_main_gametime;
extern D_80061078_t D_80061078[4];
extern vs_main_flags_t vs_main_stateFlags;
extern vs_main_inventoryIndices_t vs_main_inventoryIndices;
extern vs_main_stickPos vs_main_stickPosBuf;
int vs_main_gametimeUpdate(int vsync);
void vs_main_jumpToBattle(void);
void vs_main_showEndingAndReturnToTitle(void);
int vs_main_getRand(int);
int vs_main_getRandSmoothed(int);
int vs_main_execTitle(void);
void vs_main_exec(void);
void vs_main_enableReset(int);
void func_80042CA0(void);
void func_80042CB0(void);
int func_800433B4(char *arg0, u_int arg1, int arg2);
int func_800434A4(u_int arg0, int arg1);
void vs_main_padDisconnectAll(void);
int vs_main_updatePadState(int, char [34]);
void vs_main_padConnect(int, char [34]);
void vs_main_setVibrateParams(void);
void func_800438C8(int);
int vs_main_processPadState(void);
void vs_main_freeHeapR(void *);
void vs_main_freeHeap(void *);
void *vs_main_allocHeapR(u_int);
void *vs_main_allocHeap(u_int);
vs_main_CdQueueSlot *vs_main_allocateCdQueueSlot(const vs_main_CdFile *);
void vs_main_freeCdQueueSlot(vs_main_CdQueueSlot *);
void vs_main_cdEnqueue(vs_main_CdQueueSlot *slot, void *vram);
void vs_main_cdEnqueuePriority(vs_main_CdQueueSlot *slot, void *vram);
int vs_main_loadMusicSlot(int id, int targetSlot);
int vs_main_clearMusicLoadQueue(void);
int vs_main_loadAndWaitMusicSlot(int id, int targetSlot);
void vs_main_loadAndWaitSoundSlot(int id);
void vs_main_playSfx(int arg0, long id, int arg2, int arg3);
void vs_main_playSfxDefault(int, int);
void vs_main_stopMusic(void);
int vs_main_freeMusic(int arg0);
void vs_main_resetSound(void);
void func_80044DB4(void);
int func_80044D94(void);
void func_80044DD4(int);
int func_80044DF4(int);
int func_80044EC8(int);
int func_80044F60(int, int, u_int);
int func_80045000(int id, int arg1, int arg2);
int func_800454B8(int);
int func_80045574(int, int, int);
int func_800455AC(int, int, int);
int func_8004550C(int);
void func_80045DC0(void);
void vs_main_panSfx(int arg0, int arg1, SVECTOR *vec);
int vs_main_computeSfxPan(int coords, int z);
void func_80045BFC(int arg0, int arg1, int arg2, char arg3);
void func_80045C74(int arg0, int arg1, int arg2, char arg3);
void func_80045CEC(int, int, int, char);
int func_80046168(u_int);
int func_80046194(void);
void func_800461CC(int, u_int [], u_int, int, int);
void func_800462E8(int arg0, int *arg1, u_int arg2);
void func_8004644C(int arg0, int *arg1, int arg2);
void func_80046494(int arg0, int *arg1, int arg2, int arg3, int arg4);
void vs_main_setMonoSound(int);
int func_80046608(u_int arg0);
int func_80046634(void);
void vs_main_loadClutTransition();
void vs_main_transformClutSlot(int, int, int, short, int, int);
void func_800472D0(int arg0, D_8005DC6C_t *arg1);
void vs_main_loadSoundFile(int);
void func_800478E0(int, int, int, int, int);
void func_80047FC0(int, int, int, int, int, int);
void func_80047FFC(void);
void func_800483FC(void);
void func_800481C0(void);
void vs_main_bzero(void *, int);
void vs_main_memcpy(void *dest, void *src, u_int sz);
void vs_main_nop9(int, int);
void vs_main_nop10(int, int);
void vs_main_loadClut(const u_short *img, u_int slot, u_int colorOffset, u_int count);
void func_80048EC4(void);
void vs_main_commitClut(void);
void vs_main_setClutState(int arg0);
void vs_main_clutInit(void);
int vs_main_diskLoadFile(int sector, int bytes, void *vram);
void vs_main_resetPadAct(void);
int vs_main_streamXa(int sector, int byteCount);
int vs_main_checkStreamXaEnd(void);
int func_8004552C(int id, int arg1, int arg2);
int vs_main_getCurrentMusicId(void);
void func_80045D64(int arg0, int arg1);
void vs_main_wait(void);
void vs_main_initHeap(vs_main_HeapHeader *node, u_int value);
void vs_main_resetGame(void);
int vs_main_processSoundQueue(void);
int vs_main_setCurrentSfx(int id);
int vs_main_freeSfx(u_int);
int vs_main_freeSfxQueueSlot(u_int slot);
int vs_main_clearMusicLoadSlot(u_int);
int vs_main_loadSfxSlot(int, u_int);
typedef struct 
{
  RECT uv;
  CVECTOR colors[4];
} vs_battle_backgroundTile;
typedef struct 
{
  int width;
  int height;
  int unk8;
  vs_battle_backgroundTile tiles[0];
} vs_battle_backgroundLayout;
typedef struct 
{
  u_int unk0_0 : 8;
  u_int unk0_8 : 8;
  u_int unk0_16 : 8;
  u_int unk0_24 : 8;
} vs_battle_objectData_flags;
typedef struct 
{
  u_char dataType;
  signed char index;
  short modelId;
  void *unk4;
  u_long dataAddr;
  vs_battle_objectData_flags unkC;
  u_char actorId;
  char unk11;
  char material;
  char variant;
} vs_battle_objectData;
short vs_battle_clamp(short arg0, int arg1, int arg2);
void func_80090EEC(void);
void func_80091314(int);
void func_80091320(int);
void func_8009134C(int, int);
int func_800913BC(int);
void func_80091468(int, int);
void func_8009147C(vs_battle_backgroundLayout *, int, int, int);
void func_800918E8(int);
int func_80091998(int);
int func_800919D8(int);
int func_80091A1C(int, int);
int func_80091A78(int, int);
void func_80091B04(void);
void func_80092548(void);
int func_8009291C(int);
int func_800936F8(int);
int func_80093764(int);
void func_80093788(int);
void func_800938AC(int);
void func_80093A70(void);
void func_80093AB4(void);
int func_80093F24(int arg0);
void func_80095B70(int);
void func_8009627C(void);
void func_8009722C(void);
void func_80098160(int);
void func_80098194(int);
int vs_battle_getEmptyObjectDataSlot(void);
void func_80098B38(int, int);
int vs_battle_populateDataSlot(vs_battle_objectData *);
int vs_battle_processObjectDataQueue(void);
int func_8009CC20(int, int);
int func_8009CE9C(u_int arg0);
int func_8009CFA0(void);
int func_8009CFB0(int);
void func_8009D468();
void func_8009D854(int);
void func_8009E5C4(int);
void func_8009F314(int, void *, int);
int func_8009FD5C(int, int, int);
void func_800A0A1C(int, int);
void func_800941FC(void);
extern vs_battle_objectData vs_battle_objectDataSlots[16];
typedef struct 
{
  VECTOR position;
  VECTOR lookAt;
  VECTOR angles;
  int farClip;
} vs_camera_t;
typedef struct 
{
  void *unk0;
  void *unk4;
  void *unk8;
  void *unkC;
  u_int firstPersonViewFacing;
  MATRIX viewMatrix;
  vs_camera_t camera;
} vs_scratch_t;
extern vs_scratch_t vs_scratch;
typedef struct 
{
  short width;
  short height;
  short unk4;
  short unk6;
} _mpdRoomCollisionMapSection;
typedef struct 
{
  int unk0;
  int unk4;
  int unk8;
  int unkC;
  int unk10;
  int unk14;
  int unk18;
  int dataOffset;
  int unk20;
  int unk24;
  int unk28;
  int unk2C;
  int unk30;
  int unk34;
  int unk38;
  int unk3C;
} _mpdGeometryHeader;
typedef struct 
{
  short p1x;
  short p1y;
  short p1z;
  char p2x;
  char p2y;
  char p2z;
  char p3x;
  char p3y;
  char p3z;
  char r1;
  char g1;
  char b1;
  char type;
  char r2;
  char g2;
  char b2;
  char u3;
  char r3;
  char g3;
  char b3;
  char v3;
  char u1;
  char v1;
  short texelMask;
  char u2;
  char v2;
  short idTexture;
} _mpdPolygt3;
typedef struct 
{
  short p1x;
  short p1y;
  short p1z;
  char p2x;
  char p2y;
  char p2z;
  char p3x;
  char p3y;
  char p3z;
  char r1;
  char g1;
  char b1;
  char type;
  char r2;
  char g2;
  char b2;
  char u3;
  char r3;
  char g3;
  char b3;
  char v3;
  char u1;
  char v1;
  short texelMask;
  char u2;
  char v2;
  short idTexture;
  char p4x;
  char p4y;
  char p4z;
  char u4;
  char r4;
  char g4;
  char b4;
  char v4;
} _mpdPolygt4;
typedef struct 
{
  int numGroups;
  _mpdGeometryHeader groups[0];
} _mpdGeometrySection;
typedef struct 
{
  u_int unk0_0 : 5;
  u_int unk0_5 : 1;
  u_int unk0_6 : 1;
  u_int unk0_7 : 1;
  u_int unk0_8 : 1;
  u_int unk0_9 : 1;
  u_int unk0_10 : 3;
  u_int unk0_13 : 1;
  u_int unk0_14 : 1;
  u_int unk0_15 : 1;
  u_int unk0_16 : 1;
  u_int unk0_17 : 1;
  u_int unk0_18 : 2;
  u_int unk0_19 : 12;
} _mpdRoomSection3;
typedef struct 
{
  short unk0;
  short unk2;
  short unk4;
  short unk6;
  union 
  {
    short s16;
    struct 
    {
      char state;
      char unk1;
    } lock;
  } unk8;
  char unkA;
  char unkB;
} _mpdRoomSection9;
typedef struct 
{
  _mpdRoomSection9 unk0;
  short keyOrSigil;
  short unkE;
  short unk10;
  char unk12;
  char unk13;
} _mpdRoomSectionA;
typedef struct 
{
  int unk0[5];
  VECTOR unk14;
  int unk24;
  int unk28;
  int unk2C;
  char isWorkshop;
  char unk31;
  char trapLevel;
  char unk33;
  short unk34;
  short unk36;
  short unk38;
  short unk3A;
  short unk3C;
  short unk3E;
  short unk40;
  short unk42;
  short unk44;
  short unk46;
  union 
  {
    short s16[2];
    int s32;
  } unk48[2];
  short sceneId;
  short unk52;
  short unk54;
  short unk56;
} _mpdRoomSectionB;
typedef struct 
{
  int unk0;
  int unk4;
  int unk8;
  int *unkC;
  int *unk10;
  int unk14[0];
} _mpdRoomSectionE;
typedef struct 
{
  int visited;
  void *dataAddress;
  short zoneId;
  short mapId;
} vs_battle_room;
typedef struct 
{
  int roomCount;
  vs_battle_room rooms[0];
} vs_battle_scene;
typedef struct 
{
  u_short unk0_0 : 5;
  u_short unk0_5 : 5;
  u_short unk0_10 : 1;
  u_short unk0_11 : 3;
  u_short unk0_14 : 1;
  u_short unk0_15 : 1;
  u_short unk2;
  char unk4[0];
} _mpdRoomSection11;
typedef struct 
{
  u_int unk0_0 : 8;
  u_int unk0_8 : 1;
  u_int unk0_9 : 1;
  u_int unk0_10 : 2;
  u_int unk0_12 : 2;
  u_int unk0_14 : 2;
  u_int unk0_16 : 7;
  u_int unk0_23 : 3;
  u_int unk0_26 : 1;
  u_int unk0_27 : 2;
  u_int unk0_29 : 3;
  u_int unk4_0 : 1;
  u_int unk4_1 : 7;
  u_int unk4_8 : 2;
  u_int unk4_10 : 6;
  u_int unk4_16 : 16;
  VECTOR unk8[4];
  VECTOR unk48[6];
  int unkA8;
  int unkAC;
  int unkB0;
  _mpdRoomSection3 unkB4;
} _mpdRoomSection12;
typedef struct 
{
  short unk0;
  short unk2;
  short unk4;
  short unk6;
  short unk8;
  short unkA;
  short unkC;
  short unkE;
  short unk10;
  short unk12;
  vs_battle_loot unk14;
} _mpdRoomSection13;
typedef struct 
{
  short minX;
  short minZ;
  short maxX;
  short maxZ;
} _zoneContextBounds;
typedef struct 
{
  char unk0;
  char unk1;
  short unk2;
  struct 
  {
    u_int geometrySectionLen;
    u_int collisionMapSectionLen;
    u_int section3Len;
    u_int doorSectionLen;
    u_int lightingSectionLen;
    u_int section6Len;
    u_int section7Len;
    u_int section8Len;
    u_int section9Len;
    u_int sectionALen;
    u_int sectionBLen;
    u_int textureEffectsSectionLen;
    u_int sectionDLen;
    u_int sectionELen;
    u_int sectionFLen;
    u_int section10Len;
    u_int section11Len;
    u_int section12Len;
    u_int section13Len;
    u_int akaoSectionLen;
    u_int section15Len;
    u_int section16Len;
    u_int section17Len;
    u_int section18Len;
  } header;
  int *geometrySection;
  _mpdRoomCollisionMapSection *collisionMapSection;
  _mpdRoomSection3 *section3;
  _mpdRoomDoorSection_t *doorSection;
  int lightingSection;
  int section6;
  int section7;
  int section8;
  _mpdRoomSection9 *section9;
  _mpdRoomSectionA *sectionA;
  _mpdRoomSectionB *sectionB;
  void *textureEffectsSection;
  void *sectionD;
  _mpdRoomSectionE *sectionE;
  vs_battle_scene *sectionF;
  u_short *section10;
  _mpdRoomSection11 *section11;
  _mpdRoomSection12 *section12;
  _mpdRoomSection13 *section13;
  u_int *section14;
  int section15;
  int section16;
  int section17;
  _zoneContextBounds *section18;
} _mpdRoomSection;
typedef struct 
{
  short class[2][8];
  short affinity[2][8];
} vs_battle_classAffinityCurrent;
typedef struct 
{
  short class[8];
  u_short affinity[8];
} vs_battle_classAffinityBaseline;
typedef struct 
{
  u_short unk0[0x18];
} vs_battle_uiWeapon_unk;
typedef struct 
{
  u_short unk0;
  u_short unk2;
  char unk4;
  char unk5;
  char unk6;
  char unk7;
} func_80103080_t;
typedef struct 
{
  u_short id;
  signed char subId;
  signed char wepId;
  char category;
  signed char strength;
  signed char intelligence;
  signed char agility;
  u_short currentDp;
  u_short maxDp;
  u_short currentPp;
  u_short maxPp;
  char damageType;
  char costType;
  char cost;
  char material;
  char unk14;
  char gemSlots;
  char gemEffects;
  char index;
  vs_battle_range_t range;
  signed char types[4];
  signed char classes[8];
  signed char affinities[8];
} vs_battle_uiEquipment;
typedef struct 
{
  char name[24];
  vs_battle_uiEquipment blade;
  vs_battle_uiEquipment grip;
  vs_battle_uiEquipment gems[3];
  char wepId;
  char damageTypeValue;
  char risk;
  char unk10B;
  char dropRate;
  u_char index;
  u_char damageType;
  u_char actionType;
  u_short currentPp;
  u_short maxPp;
  u_short currentDp;
  u_short maxDp;
  short currentStr;
  u_short baseStr;
  short currentInt;
  u_short baseInt;
  short currentAgility;
  u_short baseAgility;
  vs_battle_range_t range;
  vs_battle_classAffinityCurrent classAffinityCurrent;
  short unk168[8];
  vs_battle_classAffinityBaseline classAffinityBaseline;
} vs_battle_uiWeapon;
typedef struct 
{
  int unk0[6];
  vs_battle_uiEquipment base;
  vs_battle_uiEquipment gems[3];
  char wepId;
  char dropRate;
  u_char index;
  char unkDB;
  u_short currentPp;
  u_short maxPp;
  u_short currentDp;
  u_short maxDp;
  short currentStr;
  u_short baseStr;
  short currentInt;
  u_short baseInt;
  short currentAgility;
  u_short baseAgility;
  char types[4];
  vs_battle_classAffinityCurrent classAffinityCurrent;
  short unk134[8];
  vs_battle_classAffinityBaseline classAffinityBaseline;
} vs_battle_uiShield;
typedef struct 
{
  vs_battle_uiEquipment armor;
  u_short currentDp;
  u_short maxDp;
  short currentStr;
  u_short baseStr;
  short currentInt;
  u_short baseInt;
  short currentAgility;
  u_short baseAgility;
  u_short types[4];
  vs_battle_classAffinityCurrent classAffinityCurrent;
  short unk88[8];
  char dropRate;
  char unk99;
  u_char index;
  u_char unk9B;
  vs_battle_classAffinityBaseline classAffinityBaseline;
} vs_battle_uiArmor;
typedef struct 
{
  short hp;
  short maxHp;
  signed char agilityDefenseBonus;
  char unk5;
  u_char nameIndex;
  char chainEvasion;
  short types[4];
  short affinities[8];
  vs_battle_uiArmor armor;
} vs_battle_uiEquipment_limb;
typedef struct 
{
  vs_battle_uiEquipment accessory;
  short currentStr;
  short currentInt;
  short currentAgility;
  char dropRate;
  u_char index;
  u_short types[4];
  u_short classes[8];
  u_short affinities[8];
} vs_battle_uiAccessory;
typedef struct 
{
  u_short id;
  char count;
  char unk3;
} vs_battle_uiMisc;
typedef struct 
{
  u_short id;
  char dropRate;
  char unk3;
} vs_battle_uiMiscRand;
typedef struct 
{
  char unk0[0x5E8];
} vs_battle_actor4;
typedef struct 
{
  u_short id;
  char index : 4;
  char unk2_4 : 4;
  char unk3;
} vs_battle_actor_unk8C0;
typedef struct 
{
  u_short unk0;
  char unk2_0 : 1;
  char unk2_1 : 1;
  char unk2_2 : 6;
  u_char unk3;
} vs_battle_actor2_flags;
enum vs_battle_statuses
{
  vsBattleStatusStrDown = 1 << 0,
  vsBattleStatusStrUp = 1 << 1,
  vsBattleStatusIntDown = 1 << 2,
  vsBattleStatusIntUp = 1 << 3,
  vsBattleStatusAgiDown = 1 << 4,
  vsBattleStatusAgiUp = 1 << 5,
  vsBattleStatusQuicken = 1 << 6,
  vsBattleStatusSilent = 1 << 7,
  vsBattleStatusParalysis = 1 << 8,
  vsBattleStatusPoison = 1 << 9,
  vsBattleStatusNumbness = 1 << 10,
  vsBattleStatusCurse = 1 << 11,
  vsBattleStatusRegen = 1 << 12,
  vsBattleStatusWard = 1 << 13,
  vsBattleStatusEquDown = 1 << 14,
  vsBattleStatusEquUp = 1 << 15,
  vsBattleStatusAirAtkUp = 1 << 16,
  vsBattleStatusFireAtkUp = 1 << 17,
  vsBattleStatusEarthAtkUp = 1 << 18,
  vsBattleStatusWaterAtkUp = 1 << 19,
  vsBattleStatusAirDefUp = 1 << 20,
  vsBattleStatusFireDefUp = 1 << 21,
  vsBattleStatusEarthDefUp = 1 << 22,
  vsBattleStatusWaterDefUp = 1 << 23
};
typedef struct 
{
  char name[24];
  short currentHP;
  short maxHP;
  short currentMP;
  short maxMP;
  u_short risk;
  u_short strength;
  u_short totalStrength;
  u_short intelligence;
  u_short totalIntelligence;
  u_short agility;
  u_short totalAgility;
  char unk2E;
  char unk2F;
  char unk30;
  char unk31;
  char unk32;
  char unk33;
  u_int unk34 : 8;
  u_int unk35 : 8;
  u_int unk36 : 8;
  u_int enemyClass : 3;
  u_int reach : 5;
  u_int currentRange : 8;
  u_int unk39 : 8;
  u_int unk3A : 8;
  u_int unk3B_0 : 3;
  u_int currentAttackShapeAngle : 5;
  vs_battle_uiWeapon weapon;
  vs_battle_uiShield shield;
  vs_battle_uiAccessory accessory;
  vs_battle_uiEquipment_limb limbs[6];
  vs_battle_actor_unk8C0 armor[6][4];
  char dpSplash[6][6];
  int unk944;
  int statuses;
  u_char statusTimers[8];
  u_short unk954;
  u_char unk956_0 : 1;
  u_char isAnalyzed : 1;
  u_char unk956_2 : 6;
  u_char unk957;
  vs_battle_uiMisc miscItem;
  vs_battle_uiMisc unk95C;
  vs_battle_uiMiscRand unk960;
} vs_battle_actor2;
typedef struct vs_battle_actor
{
  struct vs_battle_actor *next;
  int id;
  u_char unk8;
  u_char unk9;
  u_char unkA;
  u_char unkB;
  int unkC;
  union 
  {
    u_char u8[4];
    u_short u16[2];
    int s32;
  } unk10;
  int unk14;
  u_short unk18;
  u_short subType;
  u_short unk1C;
  u_char unk1E;
  u_char unk1F;
  int weaponDrawn;
  u_char unk24;
  u_char equippedWeaponCategory;
  u_char unk26;
  u_char unk27;
  u_char defeated;
  u_char unk29;
  u_char unk2A;
  u_char unk2B;
  union 
  {
    u_char u8[4];
    int s32;
  } unk2C;
  u_short unk30_0 : 4;
  u_short unk30_4 : 5;
  u_int unk30_9 : 5;
  u_int unk30_14 : 18;
  int unk34;
  int unk38;
  vs_battle_actor2 *unk3C;
  int unk40;
  D_800F4538_unk0 *unk44;
  vs_battle_actor4 *unk48[2];
} vs_battle_actor;
typedef struct 
{
  struct 
  {
    vs_battle_actor unk0;
    vs_battle_actor2 unk50;
  } unk0;
  D_800F4538_unk0 unk9B4;
  MATRIX unk6C[8];
  int unk16C;
  int unk170;
  int unk174;
} vs_battle_actor_dat2;
typedef struct 
{
  vs_battle_actor_dat2 unk0;
  char unkB2C[0x1788];
  vs_battle_actor4 unk22B4[2];
} vs_battle_actor_dat;
typedef struct 
{
  char id;
  char exit;
  char flags;
  char lock;
} vs_battle_door;
typedef struct 
{
  char p0;
  char p1;
  char p2;
  char p3;
} vs_battle_prim;
typedef struct 
{
  int vertexCount;
  SVECTOR vertices[0];
} vs_battle_roomVertices;
typedef struct 
{
  char name[32];
  short prev;
  short next;
} vs_battle_roomName;
typedef struct 
{
  u_short currentManual;
  u_short unk2;
  u_short scrollPositions[14];
  u_short unk20;
  char unk22;
  char unk23;
} vs_battle_manualDisplayState_t;
typedef struct 
{
  u_char unk0[0x464];
} _zndEnemy;
typedef struct 
{
  u_char type;
  u_char index;
  u_char hidden;
  u_char unk3;
  u_char id;
  u_char slot;
  u_char flagTest;
  u_char statTest;
  u_short flagValue;
  u_short statValue;
  u_char x;
  u_char direction;
  u_char z;
  u_char height;
  u_char settings[8];
  u_char unk18[2];
  u_short itemId;
  u_short item2Id;
  u_short item3Id;
  u_char itemCount;
  u_char item2Count;
  u_char item3Count;
  u_char facing;
  u_char unk24;
  u_char variant;
  u_char unk26;
  u_char sceneTest;
} _mpdEnemy;
typedef struct 
{
  int zndId;
  vs_main_CdFile *mpdLbas;
  int zndEnemyCount;
  _zndEnemy *zndEnemies;
  vs_main_CdFile *zudFiles;
  int unk14;
  vs_main_CdQueueSlot *unk18;
  void *unk1C;
  vs_main_CdQueueSlot *zndCdFile;
  void *zndData;
  int musicId;
  int musicSlot;
  int unk30;
  int mapId;
  int unk38;
  short unk3C;
  short unk3E;
  _zoneContextBounds bounds;
  int unk48;
  int unk4C;
  int unk50;
  int unk54;
  vs_main_CdQueueSlot *mpdCdFile;
  void *mpdData;
  u_int mpdEnemyCount;
  _mpdEnemy *mpdEnemies;
  int unk68;
} _zoneContext_t;
typedef struct 
{
  int unk0;
  int unk4;
  int unk8;
  int unkC;
} D_800F1A68_t;
typedef struct 
{
  u_short unk0_0 : 10;
  u_short unk0_10 : 6;
} D_800F18E8_t;
typedef struct 
{
  int yaw;
  int pitch;
  int distance;
  int mode;
} _sphericalValues;
typedef struct 
{
  _sphericalValues values;
  _sphericalValues delta;
  int initialDistance;
  int unk24;
  int speed;
} _sphericalCamera;
typedef struct 
{
  SVECTOR unk0;
  union 
  {
    char u8[2];
    u_short u16;
  } unk8;
  u_char unkA;
  u_char unkB;
  u_short unkC;
  u_char unkE;
  u_char displayTextLen;
  short unk10;
  short unk12;
  signed char displayText[4];
} D_800F1BAC_t;
typedef struct 
{
  vs_battle_uiEquipment blade;
  vs_battle_uiEquipment grip;
  vs_battle_uiEquipment gems[3];
  char material;
  char dropRate;
  short index;
  char name[24];
} vs_battle_weaponIntermediate;
typedef struct 
{
  vs_battle_uiEquipment base;
  vs_battle_uiEquipment gems[3];
  char material;
  char dropRate;
  short index;
} vs_battle_shieldIntermediate;
typedef struct 
{
  u_char titlePage;
  u_char titleRow;
  u_char encyclopaediaPage;
  u_char encyclopaediaRow;
  u_char recordTimePage;
  u_char recordTimeRow;
  u_char gazettePage;
  u_char gazetteRow;
} vs_battle_menu9CursorMemory_t;
typedef struct vs_battle_lootListNode
{
  struct vs_battle_lootListNode *next;
  int actorId;
  vs_battle_loot loot;
} vs_battle_lootListNode;
typedef struct 
{
  POLY_F4 covered;
  POLY_F4 edge;
} vs_battle_screenTransitionWipePoly;
typedef struct 
{
  vs_camera_t fromCameraState;
  _sphericalCamera fromSpherical;
  int fromNearClip;
  int fromProjectionDistance;
  int fromFarClip;
  _sphericalCamera toSpherical;
  int toNearClip;
  int toProjectionDistance;
  int toFarClip;
  VECTOR targetPosition;
  VECTOR targetLookAt;
  VECTOR direction;
  VECTOR lookAtStep;
  int farClipStep;
  int nearClipStep;
  int projectionDistanceStep;
  int transitionSpeed;
  int transitionFrames;
} vs_battle_cameraTransition_t;
typedef struct 
{
  vs_battle_uiEquipment equip;
  char material;
  char dropRate;
  char unk32;
  char index;
} _armorIntermediate;
typedef struct 
{
  vs_battle_uiEquipment equip;
  char dropRate;
  char index;
  char unk32;
  char unk33;
} _accessoryIntermediate;
typedef struct 
{
  u_short id;
  char unk2;
  char index;
} _actions;
typedef struct 
{
  u_short hp;
  char agilityDefenseBonus;
  char chainEvasion;
  signed char types[4];
  signed char affinities[8];
  _actions actions[4];
  _armorIntermediate unk20;
  char dpSplash[6];
  char unk5A[2];
} _limbInit;
typedef struct 
{
  u_short subType;
  char unk2;
  u_int reach : 5;
  u_int unk3_5 : 3;
  char name[24];
  short hp;
  short mp;
  char strength;
  char intelligence;
  char agility;
  char unk23;
  char unk24;
  char walkSpeed;
  char unk26;
  char runSpeed;
  int unk28;
  int unk2C;
  char unk30;
  char unk31;
  char unk32;
  char unk33;
  vs_battle_weaponIntermediate weapon;
  vs_battle_shieldIntermediate shield;
  _accessoryIntermediate accessory;
  _limbInit limbs[6];
  int mpdIdentifer;
} vs_battle_charInitData;
typedef struct 
{
  SVECTOR unk0;
  u_char unk8;
  u_char unk9;
  signed char unkA;
  char effect;
  short unkC;
  short unkE;
  int unk10;
  char unk14;
  char unk15;
  char unk16;
  char enemyClass;
} D_800F19CC_t5;
void func_80069C6C(int);
void func_8006CDB8(int);
void func_8006CA20(int, SVECTOR *);
void func_8006CB04(int, SVECTOR *);
void func_80069FC4(int, int);
void vs_battle_copyInventoryBladeStats(vs_battle_uiEquipment *, vs_main_inventoryBlade *);
void vs_battle_copyInventoryGripStats(vs_battle_uiEquipment *, vs_main_inventoryGrip *);
void vs_battle_copyInventoryArmorStats(vs_battle_uiEquipment *, vs_main_inventoryArmor *);
void vs_battle_copyInventoryGemStats(vs_battle_uiEquipment *, vs_main_inventoryGem *);
void vs_battle_applyWeapon(vs_battle_uiWeapon *, vs_main_inventoryWeapon *);
void vs_battle_applyShield(vs_battle_uiShield *, vs_main_inventoryShield *);
void vs_battle_applyArmor(vs_battle_uiArmor *, vs_main_inventoryArmor *);
void vs_battle_applyAccessory(vs_battle_uiAccessory *arg0, vs_main_inventoryArmor *arg1);
void vs_battle_equipWeapon(vs_main_inventoryWeapon *);
void vs_battle_equipShield(vs_main_inventoryShield *shield);
void vs_battle_equipArmor(int part, vs_main_inventoryArmor *armor);
void vs_battle_equipAccessory(vs_main_inventoryArmor *accessory);
void vs_battle_applyShieldStats(vs_battle_uiShield *, vs_battle_shieldIntermediate *);
void vs_battle_applyWeaponStats(vs_battle_uiWeapon *, vs_battle_weaponIntermediate *);
void vs_battle_copyUiBladeStats(vs_main_inventoryBlade *dropBlade, vs_battle_uiEquipment *targetBlade);
void vs_battle_copyUiGripStats(vs_main_inventoryGrip *, vs_battle_uiEquipment *);
void vs_battle_copyUiGemStats(vs_main_inventoryGem *, vs_battle_uiEquipment *);
void vs_battle_copyUiWeaponStats(vs_main_inventoryWeapon *arg0, vs_battle_uiWeapon *arg1);
void vs_battle_copyUiShieldStats(vs_main_inventoryShield *, vs_battle_uiShield *);
void vs_battle_copyUiArmorStats(vs_main_inventoryArmor *, vs_battle_uiArmor *);
void vs_battle_copyUiAccessoryStats(vs_main_inventoryArmor *, vs_battle_uiAccessory *);
void vs_battle_nop0(vs_battle_actor2 *);
void func_8006C480(int, int);
void func_8006C4A4(int, int);
int func_8006C84C(int);
void func_8006CDD8(SVECTOR *);
void func_8006CE50(void);
void func_8006CE70(SVECTOR *);
int vs_battle_getCameraRoll(void);
void vs_battle_lookAt(VECTOR *from, VECTOR *to, MATRIX *mat);
int vs_battle_getCurrentRoomId(void);
void func_800760CC(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5);
void func_80076F24(int id, vs_battle_charInitData *arg1, int arg2, int arg3, int arg4, int arg5);
vs_battle_actor *func_800774FC(int arg0, int arg1, int bladeWepId, int bladeMaterial, int shieldWepId, int shieldMaterial, vs_battle_objectData_flags *arg6, int arg7);
void func_80077EC4(void);
int vs_battle_syncCameraAnglesFromPosition(_sphericalCamera *);
int func_8007BD8C(int);
void vs_battle_setCameraRoll(int arg0);
void func_8007ACB0(void);
void func_8007B0FC(void);
void func_8007B104(void);
void func_8007B1B8(int, int, short, short, short);
void func_8007B29C(int, int, int, short, short, short);
void func_8007B344(int, int, short, short, short);
void func_8007B9A0(void);
void func_8007B9CC(void);
int func_8007B9FC(void);
void func_8007BA98(int, int, int, int);
void func_8007BBB8(int, int);
void func_8007BBD8(int, int, int);
void func_8007C4D4(void);
int func_8007C8F8(int);
void func_8007CE74(int);
int func_8007CF80(int);
void func_8007BCCC(void);
void func_8007BD3C(int);
int func_8007BDD8(int);
int func_8007BE10(int);
void func_8007BE20(int, int);
void func_8007BEC4(int);
int func_8007BEE4(void);
void func_8007BF04(void);
int func_8007BF0C(void);
void func_8007BF2C(void);
void func_8007BF4C(void);
void func_8007BFC0(void);
void func_8007C050(void);
void func_8007C0AC(int, int);
void func_8007C28C(int);
void func_8007C31C(void);
void func_8007C348(int, int);
int func_8007C36C(int arg0);
int func_8007C398(void);
void func_8007C404(void);
void func_8007C424(void);
void func_8007C444(int, int, int);
void func_8007C460(int);
void func_8007C46C(int, int, int);
void func_8007C4AC(int, int, int);
void func_8007C580(_mpdRoomDoorSection_t *, int, int);
void func_8007C654(_mpdRoomDoorSection_t *, int, int);
void func_8007C8A4(int, int);
int func_8007C928(u_int arg0, int arg1, vs_battle_objectData_flags *arg2);
void func_8007CA20(int, int, int);
void func_8007CAA4(int);
void func_8007CB84(int, int wepId);
void func_8007CBBC(int);
void func_8007CBDC(int, int, int);
void vs_battle_loadEtm(int, int, int);
void func_8007CC4C(int);
void func_8007CC6C(int);
int func_8007CCAC(void);
void vs_battle_setFarClip(int arg0, int arg1);
void vs_battle_initialiseCameraFromSpherical(VECTOR *, VECTOR *, int, int);
int func_8007CF18(int);
vs_battle_actor *func_8007CF64(int actorId);
void func_8007CFF8(int, SVECTOR *);
int func_8007D03C(int);
int func_8007D08C(int, int);
int func_8007D15C(int);
void func_8007D1A8(int, int);
int vs_battle_isActionUnlocked(u_int);
void vs_battle_setActionUnlocked(u_int);
void func_8007D340(int);
void func_8007D3F8(void);
void func_8007DF48(void);
void vs_battle_setProjectionDistance(int);
void vs_battle_getCameraPosition(VECTOR *outPosition);
void vs_battle_setCameraPosition(VECTOR *inPosition);
void vs_battle_getCameraLookAt(VECTOR *outLookAt);
void vs_battle_setCameraLookAt(VECTOR *inLookAt);
void vs_battle_setNearClip(int arg0);
void func_8007DD50(int);
void func_8007DDAC(int arg0);
void func_8007DDB8(D_800F1A68_t *arg0);
void func_8007DDD4(CVECTOR *arg0);
void func_8007DDF8(D_800F1A68_t *arg0);
void func_8007DE2C(int arg0);
void func_8007DE44(u_int arg0);
void func_8007DE5C(int arg0);
int func_8007DFF0(u_int, u_int, int);
int func_8007E0A8(u_int, u_int, int);
int func_8007E180(int arg0);
int func_8007E1C0(int arg0);
int vs_battle_itemIdIsInInventory(int);
void func_80086FA8(int, vs_battle_actor2 *);
void vs_battle_setDoorEntered(void);
int func_80089104(void);
void func_8008A4DC(int arg0);
int func_8008A4FC(void);
void func_8008A6FC(void);
int vs_battle_getActionFlags(int actorId, int actionId);
void func_8008B1FC(void *);
void func_8008B430(D_800F4538_unk58_2 *, int);
void func_8008B590(MATRIX *, int);
_mpdRoomSection3 *func_8008B764(u_int arg0, u_int arg1, int arg2);
void vs_battle_setRoomsUnk0(vs_battle_scene *);
void func_8008B6B4(void);
int func_8008B808(int, int, int, int);
int vs_battle_getCurrentSceneId(void);
int vs_battle_inWorkshop(void);
void vs_battle_setDoorEntered2(int arg0);
void func_8008C0C0(void);
void func_8008C40C(void);
void func_8008CEB0(void);
void func_8008D594(int);
void func_8008D5A0(int);
void vs_battle_parseTim(u_long *, TIM_IMAGE *);
void func_8008DF14(void);
int vs_battle_getMapCompletion(void);
void func_8008E224(int, int);
int func_8008E470(void);
int func_8008E7BC(int, int, u_int);
void func_8008E88C(void);
void func_8008EAE4(int, int);
void func_8008EAF8(int);
int *func_8008EB24(void);
void func_8008EB30(int *);
int func_8008EFCC(int);
int func_8008F0FC(void);
int func_8008F234(void);
void vs_battle_exec(void);
extern int vs_battle_mapCompletionFlags[];
extern char vs_battle_loadObjectDataState;
extern _zoneContext_t _zoneContext;
extern D_800F18E8_t D_800F18E8;
extern vs_battle_lootListNode *_lootListHead;
extern vs_battle_actor *vs_battle_actors[];
extern _sphericalCamera vs_battle_cameraCurrentSpherical;
extern vs_battle_actor *vs_battle_characterState;
extern _mpdRoomDoorSection_t D_800F1AB0;
extern u_short D_800F1BA4;
extern u_short *D_800F1BA8;
extern D_800F1BAC_t (*D_800F1BAC)[32];
extern short D_800F1BB6;
extern short vs_battle_doorEntered;
extern void *vs_battle_sceneBuffer;
extern vs_battle_manualDisplayState_t vs_battle_manualDisplayState;
extern vs_battle_menu9CursorMemory_t vs_battle_menu9CursorMemory;
extern _mpdRoomSection vs_battle_roomData;
extern DVECTOR *D_800F1CBC;
extern _mpdRoomDoorSection_t D_800F1CC8;
extern short D_800F1CD4;
extern char D_800F1CD6;
extern int vs_battle_submenuState;
extern VECTOR D_800F1CE0[2];
extern DR_MODE vs_battle_screenTransitionDrMode[];
extern int vs_battle_screenTransitionAlpha;
extern vs_battle_screenTransitionWipePoly vs_battle_screenTransitionWipePolyF4[];
extern POLY_G4 vs_battle_screenTransitionEdgePolyG4[];
extern POLY_F4 vs_battle_screenTransitionPolyF4[];
extern vs_battle_cameraTransition_t *vs_battle_cameraTransition;
extern u_int vs_battle_screenTransitionStep;
extern int vs_battle_screenTransitionEffect;
extern int vs_battle_screenTransitionWipeAngle;
extern int vs_battle_screenTransitionSpeed;
extern void *D_800F4930;
extern DR_STP D_800F1970[];
extern DR_STP D_800F1988[];
typedef struct 
{
  u_short lba;
  u_short size;
} _loadFileContext;
typedef struct 
{
  u_short type;
  u_short unk2;
  union 
  {
    struct 
    {
      u_int actor : 8;
      u_int unk1 : 8;
      u_int part : 8;
      u_int unk3 : 8;
    } actor;
    SVECTOR position;
  } value;
} actionEffectTarget;
typedef struct 
{
  u_short effectId;
  u_char count;
  u_char otherCount;
  actionEffectTarget source;
  actionEffectTarget targets[16];
  u_char others[16];
  SVECTOR position;
} actionEffectRequest;
typedef struct func_800D4910_t func_800D4910_t;
typedef int (*effectExec)(func_800D4910_t *, u_int, int);
struct func_800D4910_t
{
  struct func_800D4910_t *next;
  effectExec renderer;
  void *unk8;
};
void func_800D2698(int);
void func_800D8038(int);
void func_800D6E44(void);
void func_800D7814(void);
void func_800D78F0(void);
void vs_battle_configurePlg(int, int, int);
void vs_battle_setEffectExec(effectExec arg0, int arg1);
void func_800D7FFC(int arg0);
void vs_battle_setPlgLoadState(int);
void vs_battle_configureEffectFbLoad(int lba, int size, int suffix, int isClut, int index);
void func_800DC638(void);
int func_800DEC88(void *);
void func_800E527C(int, int, int);
void func_800E4C28(int, int, int, int);
int func_800E5154(void);
void func_800E5998(void);
void func_800E6158(void);
struct actorInitializationActor;
int func_800E6178(struct actorInitializationActor *, int);
short func_800E6694(int);
void func_800E6700(int);
void func_800E68EC(int, int);
void func_800E6EAC(void);
void func_800E6F1C(void);
void func_800E7454(vs_battle_actor *);
void func_800E7608(int);
int func_800E45B4(void);
void func_800E511C(void);
void func_800E5308(void);
int vs_battle_getMainMenuLba(void);
int vs_battle_loadEffPurge(void);
extern actionEffectRequest D_800F5230;
extern actionEffectRequest D_800F53C0;
extern int D_800F5684;
extern void *vs_battle_pfileBuf;
extern _loadFileContext vs_battle_pFileLoadContext;
enum vs_battle_warlockSpellIds_e
{
  vs_battle_spell_solidShock = 0x61,
  vs_battle_spell_lightningBolt,
  vs_battle_spell_fireball,
  vs_battle_spell_vulcanLance,
  vs_battle_spell_aquaBlast,
  vs_battle_spell_spiritSurge,
  vs_battle_spell_darkChant,
  vs_battle_spell_exorcism,
  vs_battle_spell_banish,
  vs_battle_spell_explosion,
  vs_battle_spell_thunderburst = 0x6E,
  vs_battle_spell_flameSphere = 0x72,
  vs_battle_spell_gaeaStrike = 0x76,
  vs_battle_spell_avalanche = 0x7A,
  vs_battle_spell_radialSurge = 0x7E,
  vs_battle_spell_meteor = 0x82,
  vs_battle_spell_drainHeart = 0x5E,
  vs_battle_spell_drainMind = 0x5F
};
enum vs_battle_menuItemScrollable
{
  menuItem_scrollNone,
  menuItem_scrollUp,
  menuItem_scrollDown
};
enum vs_battle_menuItemState
{
  menuItemStateInactive,
  menuItemStateIdle,
  menuItemStateSlideX,
  menuItemStateSlideY,
  menuItemStateSlideXY,
  menuItemStateDismiss = 5
};
typedef struct 
{
  u_char state;
  u_char w;
  u_char backgroundWidth;
  u_char animationStep;
  u_char gradientState;
  u_char isScrollable;
  u_char selected;
  u_char unselectable;
  u_char buttonId;
  u_char rowIcon;
  u_char fontColor;
  u_char invertGradient;
  u_char material;
  u_char outsetIcon;
  u_short itemPage;
  u_short count;
  u_short max;
  short x;
  short y;
  short targetPosition0;
  short targetPosition1;
  char text[32];
  char *subText;
} vs_battle_menuItem_t;
enum vs_battle_menuStates
{
  menuStateMainMenu,
  menuStateNoneClosing = 0x1F,
  menuStateNoneClosed = 0x3F,
  menuStateOpenedFromMainMenu = 0x40
};
typedef struct 
{
  char currentMenu;
  char returnState;
  char executeAbilityType;
  char loading;
  vs_main_CdQueueSlot *cdQueue;
} vs_battle_menuState_t;
typedef struct 
{
  u_int unk0_0 : 4;
  u_int unk0_4 : 2;
  u_int unk0_6 : 2;
  u_int unk0_8 : 4;
  u_int unk0_12 : 4;
  u_int arrowAnimFrame : 8;
  u_int unk0_24 : 6;
  u_int done : 1;
  u_int unk0_31 : 1;
} vs_battle_textBox_fields;
typedef struct 
{
  vs_battle_textBox_fields unk0;
  short unk4;
  short unk6;
  char *string;
  u_short speed;
  u_short prevChunksPrinted;
  short xIndent;
  short yIndent;
  short x;
  short y;
  short centerX;
  short centerY;
  short state;
  u_char brightness;
  u_char unk1F;
  char charsPerLine;
  char lineCount;
  char unk22;
  char unk23;
  short unk24;
  short unk26;
  short unk28;
  short unk2A;
  short unk2C;
  short unk2E;
} vs_battle_textBox;
typedef struct 
{
  char cursorMemories[52];
  char unk34[6];
  struct 
  {
    char currentItemCategory;
    char unk1;
    union 
    {
      struct 
      {
        u_char index;
        u_char page;
      } cursor;
      u_short value;
    } unk2[8];
  } unk3A;
  char unk4C[4];
  char unk50;
  char unk51[51];
  char selectedEquipment;
  char unk85[25];
  u_short unk9E;
  char unkA0[0x12];
  char unkB2[0x40];
} D_800F4EE8_t;
typedef struct 
{
  short unk0;
  short unk2;
  union 
  {
    struct 
    {
      u_int unk0_0 : 9;
      u_int unk0_9 : 5;
      u_int unk0_14 : 5;
      u_int unk0_19 : 13;
    } fields;
    u_short u16[2];
    int s32;
  } executeAbility;
} D_800F18EC_t;
typedef struct 
{
  int integers[10];
  char *strings[10];
} vs_battle_stringContext_t;
typedef struct 
{
  short unk0[6];
  short unkC[2];
  u_char unk10;
  u_char unk11;
  u_char unk12;
  u_char unk13;
  u_char unk14;
  u_char unk15;
  u_char unk16[4];
} pFileBlock9;
typedef struct 
{
  u_short flags;
  u_short unk2;
  u_char unk4;
  u_char curve;
  u_char unk6;
  u_char unk7;
  u_char pad8[2];
  u_short mode;
  u_char padC[2];
  u_short limit : 8;
  u_short radius : 4;
  u_short radiusJitter : 4;
  u_short unk10;
  u_short shape12;
  short unk14[6];
  short unk20[6];
  short unk2C[6];
} func_800D0B30_t;
typedef struct 
{
  u_short count;
  u_short dataOffset;
  u_char curveSizes[0];
} pFileCurveBlock;
typedef struct 
{
  short unk0;
  short unk2;
} func_800CFE1C_t;
typedef struct 
{
  u_short unk0;
  u_short dataIndex;
} pFileTextureInfo;
typedef struct 
{
  int unk0;
  short unk4;
  u_char unk6;
  u_char unk7;
  short unk8;
  short unkA;
  short unkC;
  u_short unkE;
  int unk10;
  short unk14;
  u_char textureIndex;
  u_char unk17;
  u_char *unk18;
  pFileTextureInfo *textureInfo;
} func_800D6CF_t;
typedef struct 
{
  u_char unk0;
  u_char unk1;
  u_char unk2;
  u_char unk3;
  u_int flags : 26;
  u_int transparencyCurve : 6;
  u_char unk8;
  u_char unk9;
  u_char unkA;
  u_char unkB;
  u_char unkC;
  u_char unkD;
  u_char unkE;
  u_char unkF;
  u_char unk10;
  u_char unk11;
  u_char unk12;
  u_char unk13;
  u_char unk14;
  u_char unk15;
  u_char unk16;
  u_char unk17;
  short unk18[6];
  short unk24[6];
  short unk30[2];
  short unk34[8][6];
  short unk94[5][4];
  short unkBC[2];
  short unkC0[2];
  u_char rCurve;
  u_char gCurve;
  u_char bCurve;
  u_char unkC7;
  u_char unkC8;
  u_char unkC9;
  u_char unkCA;
  u_char unkCB;
} func_800FA098_arg0;
typedef union 
{
  int packed;
  struct 
  {
    u_char rCurve;
    u_char gCurve;
    u_char bCurve;
    u_char age;
  } fields;
} func_800D2904_curves;
typedef struct 
{
  VECTOR unk0;
  VECTOR unk10;
  int unk20;
  u_int flags;
  u_short unk28;
  u_short unk2A;
  SVECTOR unk2C;
  VECTOR unk34;
  VECTOR unk44;
  u_char unk54[0x10];
  MATRIX unk64;
  u_char unk84[0x14];
  VECTOR unk98;
  VECTOR unkA8;
  VECTOR unkB8;
  VECTOR unkC8;
  VECTOR unkD8;
  VECTOR unkE8;
  SVECTOR unkF8;
  SVECTOR unk100;
  SVECTOR unk108;
  SVECTOR unk110;
  int unk118[2];
  int unk120;
  int unk124[2];
  int unk12C[2];
  int unk134[2];
  SVECTOR unk13C;
  SVECTOR unk144;
  int unk14C[2];
  func_800D2904_curves unk154;
  u_char unk158[0x10];
  int unk168;
  int unk16C;
  int unk170;
  int unk174;
  int unk178;
  int unk17C;
  u_char unk180;
  int unk184;
  int unk188;
} func_800FA098_arg1;
typedef struct 
{
  int count;
  func_800FA098_arg0 unk4[0];
} pFileBlock5;
typedef struct 
{
  int count;
  u_short offsets[0];
} pFileBlock3;
typedef struct 
{
  short clut;
  short tpage;
  u_char u;
  u_char v;
  u_char w;
  u_char h;
  short x0;
  short y0;
  short x1;
  short y1;
  short x2;
  short y2;
  short x3;
  short y3;
} pFileTexture;
typedef struct 
{
  int unk0;
  int unk4;
  pFileBlock5 *block5Data;
  u_char *curves[32];
  pFileCurveBlock *curveBlock;
  char *block8Data;
  int block1TableCount;
  pFileTextureInfo *textureInfoTable[4];
  pFileTexture *textures;
  int block2Count;
  u_short *block2Data;
  pFileBlock9 *block9Data;
  func_800D0B30_t *block10Data;
  pFileBlock3 *block3Data;
  u_char *block11Data;
  void *block12Data;
  void *block13Data;
  int unkCC;
  actionEffectRequest *unkD0;
} D_800F569C_t;
typedef struct 
{
  VECTOR position;
  VECTOR lookAt;
  int unk20;
  int unk24;
  int unk28;
  int unk2C;
  int roll;
  int nearClip;
  int projectionDistance;
  int farClip;
} D_800F54D8_t;
typedef struct func_800D2904_t
{
  struct func_800D2904_t *previous;
  struct func_800D2904_t *next;
  short unk8;
  short unkA;
  int unkC;
  int unk10;
  int unk14;
  int unk18[3];
  int unk24[3];
  int unk30[3];
  func_800D6CF_t unk3C;
  int unk5C;
  u_int unk60_0 : 8;
  u_int unk60_8 : 4;
  u_int unk60_12 : 4;
  short unk62[3];
  short lifetime;
  u_char unk6A;
  u_char unk6B;
  u_char endEvent;
  u_char tickEvent;
  char unk6E[2];
  int unk70;
  u_char unk74[3];
  u_char unk77;
} func_800D2904_t;
typedef struct 
{
  int unk0;
  int unk4;
  int unk8;
  int unkC;
  int unk10;
  int unk14;
  int unk18;
  int unk1C;
  int unk20;
  int unk24;
  int unk28;
  int unk2C;
  int unk30;
  u_char unk34;
  u_char unk35;
  u_char unk36;
  u_char unk37;
  MATRIX unk38;
  MATRIX unk58;
  u_char unk78[0x46];
  short unkBE;
  u_char unkC0[0x10];
} func_800CE714_t2_2;
typedef struct 
{
  u_short unk0_0 : 8;
  u_short unk0_8 : 8;
} func_800D5780_t2;
typedef struct 
{
  u_long *unk0;
  func_800D5780_t2 unk4;
  short unk6;
  short unk8;
  short unkA;
  int unkC;
} func_800D5780_t;
typedef struct 
{
  char unk0;
  char unk1;
  short unk2;
  int unk4;
  func_800D5780_t unk8[0];
} D_800F53B8_t4;
typedef struct 
{
  actionEffectTarget unk0;
  actionEffectTarget unkC;
  actionEffectTarget unk18;
  actionEffectTarget unk24;
  D_800F53B8_t4 *unk30;
  func_800D4910_t *unk34;
  short unk38;
  short unk3A;
  int unk3C;
} D_800F53B8_t3;
typedef struct D_800F53B8_t
{
  struct D_800F53B8_t *next;
  struct D_800F53B8_t *previous;
  char unk8;
  char unk9;
  u_short unkA;
  char *unkC;
  char unk10[4];
  u_int unk14_0 : 8;
  u_int unk14_8 : 3;
  u_int unk14_11 : 5;
  u_int unk14_16 : 16;
  func_800D2904_t *unk18;
  func_800CE714_t2_2 unk1C[16];
  D_800F53B8_t3 unkD1C;
} D_800F53B8_t;
typedef struct 
{
  u_char unk0[0xC];
  VECTOR unkC;
  u_char unk1C[0x44];
  u_int unk60 : 8;
  u_int unk60_8 : 4;
  u_int unk60_12 : 20;
  u_char unk64[20];
} func_800FB4C0_t;
enum vs_battle_limbStatus
{
  limbStatusCritical,
  limbStatusDamaged,
  limbStatusWounded,
  limbStatusGood,
  limbStatusExcellent
};
int func_800C4794(SVECTOR *);
int func_800C58F8(u_char *);
int vs_battle_setTextBox(int, char *);
int vs_battle_selectTextBox(u_char);
char *vs_battle_printf(char *, char *);
void vs_battle_renderTextRawColor(const char *text, int xy, int color, u_long *nextPrim);
void vs_battle_renderTextRaw(const char *text, int xy, void *nextPrim);
int vs_battle_printVariableWidthFontChar(u_int glyphId, int x, int y, u_long *ot);
void vs_battle_setFontStyle(int arg0);
void vs_battle_loadGim(int, int);
void func_800C8550(u_int, void *, u_char *);
void func_800C86AC(void);
int vs_battle_loadMenuPrg(int arg0);
enum vs_battle_informationTextBoxHeader
{
  informationTextBoxHeaderNone,
  informationTextBoxHeaderCaution,
  informationTextBoxHeaderInformation,
  informationTextBoxHeaderCheck
};
void vs_battle_initInformationTextBox(enum vs_battle_informationTextBoxHeader flags);
int vs_battle_uiGradientStop(u_int gradient, u_int colorIndex, int brightness);
vs_battle_menuItem_t *vs_battle_getMenuItem(int row);
vs_battle_menuItem_t *vs_battle_setMenuItem(int row, int x, int y, int w, int backgroundWidth, char *text);
void vs_battle_renderMenuItem(vs_battle_menuItem_t *);
int func_800C930C(int);
void func_800C97BC(void);
int vs_battle_renderValue(int font, int xy, int value, u_long *before);
void vs_battle_renderStatBar(int colorIndex, int w, u_long *before, int xy);
int vs_battle_getStatusFlags(vs_battle_actor2 *);
int vs_battle_getLimbStatus(vs_battle_uiEquipment_limb *);
void func_800CAB40(void);
int func_800CABE0(int);
int vs_battle_isSpellClassUnlocked(int spellClass);
int vs_battle_chainAbilitiesUnlocked(int);
void func_800CB23C(void);
void func_800CB660(int arg0);
D_800F18EC_t *func_800CB66C(void);
void func_800CB114(void);
void func_800CB158(vs_battle_lootListNode *);
void func_800CB18C(vs_battle_loot *);
void func_800CB1C0(int);
void _setArtAndAbilityToUnlock(int art, int battleAbility);
void vs_battle_displaySceneMessage(int, int, int);
int func_800CB45C(void);
void func_800CB50C(void);
void func_800CB550(void);
void func_800CB560(void);
void func_800CB654(int);
void func_800CB7DC(void);
void vs_battle_rMemzero(void *arg0, int arg1);
void vs_battle_rMemcpy(void *dest, const void *src, int size);
int vs_battle_toBCD(int value);
int vs_battle_drawCursor(int animStep, int position);
u_int vs_battle_keystreamBits(int value);
int vs_battle_decreaseMiscCount(int);
void vs_battle_addTile(u_long *before, int rgb0, int xy, int wh);
void vs_battle_insertTpage(int arg0, u_long *arg1);
vs_battle_textBox *vs_battle_getTextBox(int arg0);
void vs_battle_initTextBox(int id, int flags, int x, int y, int w, int h, int centerX, int centerY);
int vs_battle_dismissTextBox(int);
void vs_battle_insertTpage(int, u_long *);
void vs_battle_renderImage(int xy, void *buffer, int wh);
void func_800CE64C(void);
int func_800CEEBC(void);
int func_800CEF74(int);
int func_800CF0E8(actionEffectRequest *, int, int);
void func_800CF1A8(void);
int func_800CF3F8(actionEffectRequest *, int);
int func_800CF48C(void);
void func_800CF7A8(int, int, int, int);
void func_800CF830(int, int);
void func_800CF920(void);
int vs_battle_getTextLineLength(char *str);
int vs_battle_validateShortcutSelection(int shortcut);
void func_800C8778(void);
void func_800C9F88(void);
void func_800CA2DC(void);
int func_800CACD0(int menuState, int arg1);
void func_800CB83C(void);
int func_800CF060(void);
void vs_battle_addVecToSvec(VECTOR *arg0, SVECTOR *arg1, VECTOR *arg2);
void vs_battle_lerp2DVector(short *src, int t, int *vec);
void vs_battle_lerpSvector(short *src, int t, SVECTOR *vec);
void vs_battle_lerpVector(short *src, int t, VECTOR *vec);
int vs_battle_randUniformInt(int, int);
int func_800CFE1C(short *arg0, int arg1);
void func_800D6D24(func_800D6CF_t *arg0);
int vs_battle_sampleCurve(int curveId, int distance);
void vs_battle_splineInterpolate(SVECTOR *p0, SVECTOR *p1, SVECTOR *p2, SVECTOR *p3, int t, SVECTOR *out);
void vs_battle_svecToVec(SVECTOR *arg0, VECTOR *arg1);
void vs_battle_vecToSvec(VECTOR *arg0, SVECTOR *arg1);
void func_800D2ADC(D_800F53B8_t *, int, int, int, func_800FB4C0_t *);
void func_800D6CCC(int *arg0);
void func_800D6CF0(func_800D6CF_t *arg0, int textureIndex, int arg2);
extern u_short vs_battle_menuStrings[];
extern char *vs_battle_limbNames[];
extern char vs_battle_characterWidths[];
extern u_short vs_battle_statusStrings[];
extern char D_800EB9AE;
extern char D_800EB9AF;
extern int D_800EB9B0;
extern vs_battle_lootListNode *D_800EB9C4;
extern vs_battle_loot *D_800EB9C8;
extern vs_battle_menuItem_t *vs_battle_menuItems;
extern char vs_battle_rowAnimationSteps[];
extern int D_800EBBEC[];
extern char D_800EBBFC[];
extern char D_800EBC00[];
extern u_short vs_battle_statusIconTexOffsets[];
extern u_int D_800EBC54[];
extern char vs_battle_animationIndices[];
extern char vs_battle_warlockSpellIds[];
extern char vs_battle_shamanSpellIds[];
extern char vs_battle_sorcererSpellIds[];
extern char vs_battle_enchanterSpellIds[];
extern char vs_battle_cursorBrightnessAnimation[];
extern char vs_battle_chainAbilityOffsets[];
extern char vs_battle_defenseAbilityOffsets[];
extern char vs_battle_shortcutInvoked;
extern char D_800F4E6B;
extern int *vs_battle_rowTypeBuf;
extern char vs_battle_unlockedBreakArt;
extern char *vs_battle_stringBuf;
extern char vs_battle_submenuStates[16];
extern char vs_battle_lowerScreenUiState;
extern D_800F18EC_t D_800F4E98;
extern int D_800F4EA0;
extern D_800F4EE8_t D_800F4EE8;
extern char vs_battle_unlockedBattleAbility;
extern char D_800F4FDB;
extern D_800F569C_t *D_800F569C;
extern D_800F569C_t D_800F56C8;
extern vs_battle_textBox vs_battle_textBoxes[];
extern u_char vs_battle_miscItemInvoked;
extern vs_battle_stringContext_t vs_battle_stringContext;
extern vs_battle_menuState_t vs_battle_menuState;
extern SVECTOR D_800F5310[];
extern D_800F53B8_t *D_800F53BC;
extern effectExec vs_battle_effectRenderers[];
int vs_gte_rsqrt(int);
extern void * const vs_overlay_slots[5];
void vs_overlay_getSp(int *);
void vs_overlay_jumpToTitle(int *);
void vs_overlay_jumpToBattle(int *);
void vs_overlay_wait(void);
extern int abs(int);
extern int atoi(char *);
extern long atol(char *);
extern long strtol(char *, char **, int);
extern unsigned long strtoul(char *, char **, int);
extern long labs(long);
extern void InitHeap(unsigned long *, unsigned long);
extern void free(void *);
extern void *malloc(size_t);
extern void *calloc(size_t, size_t);
extern void *realloc(void *, size_t);
extern void InitHeap2(unsigned long *, unsigned long);
extern void free2(void *);
extern void *malloc2(size_t);
extern void *calloc2(size_t, size_t);
extern void *realloc2(void *, size_t);
extern void InitHeap3(unsigned long *, unsigned long);
extern void free3(void *);
extern void *malloc3(size_t);
extern void *calloc3(size_t, size_t);
extern void *realloc3(void *, size_t);
extern void qsort(void *, size_t, size_t, int (*)());
extern int rand(void);
extern void srand(unsigned int);
extern void *bsearch(unsigned char *, unsigned char *, size_t, size_t, int (*)());
extern void exit();
typedef struct 
{
  u_short unk0_0 : 5;
  u_short unk0_5 : 5;
  u_short unk0_10 : 6;
} D_800F5910_t;
typedef struct 
{
  u_char unk0;
  u_char unk1;
  u_char unk2;
} func_800DFA54_t;
typedef struct 
{
  short unk0;
  short unk1;
  short unk2;
  short unk3;
} func_8008D2C0_t;
typedef struct 
{
  signed char dx : 2;
  signed char unk0_2 : 2;
  signed char dz : 2;
  signed char unk0_6 : 2;
} D_800F16EC_t;
typedef struct 
{
  char unk0[8];
  u_char unk8;
  u_char unk9;
  char unkA;
  u_int unkB_0 : 2;
  u_int unkB_2 : 2;
  u_int unkB_4 : 4;
  u_short unkC;
  u_short unkE;
  SVECTOR unk10;
  char unk18[0x24];
  vs_battle_actor2 *unk3C;
} func_800DEB10_t2;
typedef struct 
{
  int unk0;
  u_int unk4;
} func_800E78F4_t2;
typedef struct func_800E78F4_t
{
  char unk0[0x4E];
  u_short unk4E;
  char unk50[4];
  func_800DEB10_t2 *unk54;
  char unk58[4];
  vs_battle_actor2 *unk5C;
  char unk60[0xCF];
  u_char unk12F;
  char unk130[0x70];
  struct func_800E78F4_t *unk1A0;
  char unk1A4[0x28C];
  func_800E78F4_t2 *unk430;
  char unk434[0x3C];
  int unk470;
} func_800E78F4_t;
typedef struct 
{
  u_char maxX;
  u_char maxZ;
  u_char minX;
  u_char minZ;
  char unk4[0x10];
  int unk14;
  u_short unk18;
  char unk1A[2];
  func_800E78F4_t *unk1C;
  func_800E78F4_t *unk20;
  int unk24;
} D_800F58BC_t;
typedef struct 
{
  int unk0;
  int unk4;
  u_int unk8_0 : 21;
  u_int unk8_21 : 1;
  u_int unk8_22 : 10;
} func_800E1238_t;
typedef struct 
{
  char unk0[7];
  u_char unk7;
  char unk8[0x2C];
  u_char unk34;
  char unk35;
  u_char unk36;
  char unk37[0x17];
  short unk4E;
  char unk50[8];
  func_800E1238_t *unk58;
  char unk5C[0x62];
  short unkBE;
  char unkC0[0xBC];
  int unk17C;
  int unk180;
  int unk184;
  short unk188;
  char unk18A[0x9E];
  int unk228;
} func_800E0850_t;
typedef struct 
{
  int unk0;
  u_int unk4_0 : 3;
  u_int unk4_3 : 2;
  u_int unk4_5 : 8;
  u_int unk4_13 : 19;
  int unk8;
  u_int unkC_0 : 4;
  u_int unkC_4 : 3;
  u_int unkC_7 : 12;
  u_int unkC_19 : 8;
  u_int unkC_27 : 5;
} func_800DCAA0_t;
typedef struct 
{
  char unk0[0x16];
  signed char unk16;
  char unk17[0x21];
  int unk38;
  char unk3C[0x18];
  func_800DEB10_t2 *unk54;
  char unk58[0x42];
  short unk9A;
  char unk9C[0x5F];
  u_char unkFB;
  char unkFC[0x2E];
  u_char unk12A;
  char unk12B[0xD];
  u_short unk138;
  char unk13A[0x2F6];
  func_800DCAA0_t *unk430[16];
  int unk470;
} func_800DEEA4_t2;
int func_8008D2C0(func_8008D2C0_t *);
void func_800DEEFC(func_800E0850_t *, int);
struct directionalCacheState;
int func_800E0678(struct directionalCacheState *, int);
struct movementCheckState;
int func_800E0918(struct movementCheckState *, int, int);
typedef struct movementRecoveryState movementRecoveryState;
void func_800E2CCC(movementRecoveryState *);
void func_800E3600(func_800E0850_t *, int, int);
int func_800E42EC(int, int);
int func_800E4764(func_800E0850_t *, int *, int *);
void func_800E4B18(func_800E0850_t *);
void func_800E678C(func_800E0850_t *);
extern D_800F58BC_t *D_800F58BC;
extern D_800F5910_t *D_800F5910;
void func_800D8260(func_800DEEA4_t2 *arg0, int arg1, int arg2);
typedef union 
{
  unsigned int raw;
  struct 
  {
    unsigned int x : 8;
    unsigned int y : 8;
    unsigned int z : 8;
    unsigned int status : 8;
  } p;
} vs_battle_movementPosition;
typedef struct 
{
  unsigned char pad[7];
  unsigned char disabled;
  unsigned char rest[0x2C];
  vs_battle_movementPosition position;
  char unk38[0x130];
  vs_battle_movementPosition destination;
} func_800D8400_t;
int func_800D96C8(func_800D8400_t *, unsigned int, unsigned int, int);
int func_800D954C(func_800D8400_t *, unsigned int);
unsigned int func_800E4660(int, int, int);
void func_800E50A0(void *, int, int);
int func_800E45D4(int);
void func_800E1388(func_800D8400_t *, vs_battle_movementPosition, int, unsigned int);
int func_800E45D4(int);
void func_800DF9A8(func_800DFA54_t *arg0, func_800DFA54_t *arg1);
void func_800DFA54(int arg0, int arg1, int arg2, func_800DFA54_t *arg3);
int func_800DFAF8(int arg0, u_int arg1, int arg2);
typedef struct routeEdgeState
{
  char unk0[5];
  u_char unk5;
  char unk6[0x2E];
  vs_battle_movementPosition tile;
  char unk38[0x14];
  short x;
  short y;
  short z;
  char unk52[0x36];
  u_char id;
  char unk89[0xB1];
  u_short cooldown;
  u_short period;
  char unk13E[0x16];
  u_char area;
  char unk155[7];
  u_char target;
  char unk15D[0xB];
  vs_battle_movementPosition destination;
  char unk16C[4];
  vs_battle_movementPosition next;
  char unk174[8];
  int moveX;
  int moveZ;
  char unk184[4];
  short heading;
  short angle;
  char unk18C[0xF];
  u_char gate;
  func_800DFA54_t exit;
} routeEdgeState;
typedef struct 
{
  char unk0[0x60];
  short *position;
} routeEdgeActor;
typedef struct 
{
  char unk0[14];
  u_char closedGates;
} routeEdgeGlobals;
typedef u_short routeEdgePair[2];
extern u_char D_800F175C[];
int func_8008E320(int);
void func_800E4BE0(int);
inline static u_int routeWord(routeEdgePair *pair)
{
  return *((u_int *) pair);
}

inline static u_short routeHalf(routeEdgePair *pair, int index)
{
  return (*pair)[index];
}

inline static u_int routeTile(int z, int x)
{
  return (*((u_short (**)[32]) 0x1F8003C0))[z][x];
}

inline static int routeSetArea(routeEdgeState *state, vs_battle_movementPosition position)
{
  state->area = routeTile(position.p.z, position.p.x) & 15;
  return 6;
}

int func_800DFBCC(routeEdgeState *state, int index, int area);
typedef struct 
{
  short x;
  short y;
  short z;
  short pad;
} obstructionVector;
typedef struct 
{
  char pad[0x1C];
  obstructionVector position;
} obstructionModel;
typedef struct 
{
  char pad0[0x4C];
  obstructionVector position;
  char pad54[4];
  obstructionModel *model;
  char pad5C[0x2C];
  unsigned char id;
  char pad89[9];
  unsigned short radius;
  char pad94[0x1F];
  unsigned char step;
  char padB4[4];
  short height;
} obstructionState;
typedef struct 
{
  unsigned char p0;
  unsigned char p1;
  unsigned char tested;
  unsigned char p3;
  unsigned char result[8];
} obstructionCache;
int func_800E02B4(obstructionState *state, int direction, int ahead);
typedef union 
{
  unsigned int raw;
  struct 
  {
    unsigned char x;
    unsigned char y;
    unsigned char z;
    unsigned char w;
  } p;
} directionalCachePoint;
typedef struct directionalCacheState
{
  char pad[0x34];
  directionalCachePoint tile;
  directionalCachePoint previous;
  char pad3C[0x82];
  short minimumHeight;
} directionalCacheState;
int func_800E42EC(int, int);
typedef struct 
{
  unsigned char tested;
  unsigned char valid;
} directionalCacheBits;
int func_800E0678(directionalCacheState *state, int direction);
int func_800E0850(func_800E0850_t *arg0, u_int arg1);
typedef struct 
{
  char pad0[8];
  unsigned int flags;
} movementCheckObject;
typedef struct movementCheckState
{
  char pad0[0x34];
  int position;
  char pad38[0x20];
  movementCheckObject *object;
  char pad5C[0x62];
  short height;
  char padC0[0x9E];
  unsigned short flags;
  char pad160[8];
  int target;
} movementCheckState;
void func_800E4B2C();
void func_800E4B70();
unsigned int func_800E45F4(unsigned int, unsigned int);
int func_800E42E0(unsigned int);
void func_800E4B10(void *);
int func_800E0918(movementCheckState *state, int action, int mode);
typedef union 
{
  unsigned int raw;
  struct 
  {
    short x;
    short y;
  } p;
} movementStepPosition;
typedef struct 
{
  unsigned char unk0;
  char pad1[6];
  unsigned char unk7;
  char pad8[0x2C];
  directionalCachePoint tile;
  char pad38[0x14];
  movementStepPosition position;
  short z;
  char pad52[6];
  movementCheckObject *object;
  char pad5C[0x60];
  short clearance;
  short height;
  char padC0[0x25];
  unsigned char actorId;
  char padE6[0x82];
  directionalCachePoint destination;
  char pad16C[0x10];
  int moveX;
  int moveZ;
  int moveY;
  short angle;
  char pad18A[0x9E];
  int unk228;
} movementStepState;
typedef struct 
{
  char pad0[0x1E];
  short y;
  char pad20[0x3C];
  directionalCachePoint tile;
  char pad60[0x5DE];
  unsigned short height;
} movementStepActor;
int func_800E02B4(obstructionState *, int, int);
int func_800E6A6C(int, int, int, int);
void func_800E1238(func_800E0850_t *, int);
void func_800E0A68(movementStepState *state, int action);
void func_800E1238(func_800E0850_t *arg0, int arg1);
void func_800E1388(func_800D8400_t *state, vs_battle_movementPosition source, int angle, unsigned int distance);
typedef struct 
{
  char pad0[0xD];
  unsigned char flagD;
  char padE[0x26];
  vs_battle_movementPosition position;
  char pad38[0x10];
  vs_battle_movementPosition previous;
  short x;
  char pad4E[2];
  short z;
  char pad52[0x3A];
  unsigned short attempts;
  char pad8E[0x26];
  int distance;
  char padB8[0xA6];
  unsigned short flags;
  char pad160[8];
  vs_battle_movementPosition destination;
  char pad16C[4];
  vs_battle_movementPosition next;
  char pad174[8];
  int referenceX;
  int referenceZ;
} movementDestinationState;
unsigned int func_800E4660(int, int, int);
unsigned int func_800E45F4(unsigned int, unsigned int);
void func_800E4B2C(movementDestinationState *, int);
void func_800E153C(movementDestinationState *state, int unused);
void func_800E1764(movementDestinationState *state, int angle);
u_char func_800E1850(u_char arg0, u_int arg1, int arg2);
typedef union 
{
  unsigned int raw;
  struct 
  {
    unsigned char x;
    unsigned char y;
    unsigned char z;
    unsigned char w;
  } p;
} terrainMovementPoint;
typedef struct terrainMovementActor
{
  char pad0[9];
  unsigned char mode;
  char padA[2];
  unsigned short angle;
} terrainMovementActor;
typedef struct terrainMovementState
{
  char pad0[2];
  unsigned char f2;
  char pad3[1];
  unsigned char f4;
  unsigned char f5;
  char pad6[20];
  unsigned char f1A;
  char pad1B[14];
  unsigned char f29;
  char pad2A[10];
  terrainMovementPoint tile;
  char pad38[20];
  short position[3];
  char pad52[2];
  terrainMovementActor *actor;
  char pad58[272];
  terrainMovementPoint destination;
  char pad16C[16];
  int targetX;
  int targetZ;
  char pad184[4];
  short angle;
} terrainMovementState;
struct routeEdgeState;
int func_800DFBCC(struct routeEdgeState *, int, int);
void func_800E4BB8(terrainMovementState *);
void func_800E4B70(terrainMovementState *, int);
void func_800E1908(terrainMovementState *state);
void func_800E19FC(terrainMovementState *state);
typedef struct 
{
  char pad0[8];
  unsigned int flags;
} wanderObject;
typedef struct wanderState
{
  char pad0[2];
  unsigned char flag2;
  unsigned char flag3;
  char pad4[4];
  unsigned char flag8;
  char pad9[0x2B];
  vs_battle_movementPosition tile;
  char pad38[0x10];
  vs_battle_movementPosition saved;
  short x;
  short y;
  short z;
  char pad52[6];
  wanderObject *object;
  char pad5C[0x58];
  unsigned int moveArg;
  char padB8[8];
  unsigned short mode;
  unsigned short timer;
  int rangeSquared;
  char padC8[8];
  short angle;
  unsigned short cooldown;
  unsigned short restPeriod;
  unsigned short movePeriod;
  unsigned char rate;
  char padD9[0x43];
  vs_battle_movementPosition goal;
  char pad120[13];
  unsigned char mode12D;
  char pad12E[0x2F];
  unsigned char count;
  unsigned short flags;
  char pad160[8];
  vs_battle_movementPosition dest;
} wanderState;
int func_800E4690(unsigned int, int);
int D_800D9B6C(wanderState *, int);
inline static D_800F16EC_t *wanderDirection(int index)
{
  return ((D_800F16EC_t *) 0x1F8003EC) + index;
}

int func_800E1BB8(wanderState *state, unsigned int tile, unsigned int maxCount, int arg3);
inline static int wanderRandomHeading(int direction)
{
  int offset = func_800E45D4(512);
  int angle = direction << 9;
  offset += 0xFF00;
  return angle + offset;
}

inline static void wanderStartWalk(wanderState *s)
{
  s->angle = -1;
  s->count = 0;
  s->cooldown = 60;
  s->mode++;
  func_800E2CCC((movementRecoveryState *) s);
}

void func_800E1EAC(wanderState *state, unsigned int tile, unsigned int range);
typedef struct 
{
  char pad0[0x34];
  vs_battle_movementPosition tile;
  char pad38[0xC8];
  unsigned char index;
  unsigned char row;
  unsigned char rate;
  unsigned char direction;
  unsigned char timer;
  char pad105[0x63];
  vs_battle_movementPosition dest;
} patrolWaypointState;
extern unsigned short (*D_800F58D8)[8];
void func_800E50A0(void *, int, int);
inline static unsigned int patrolWaypoint(unsigned short *row, int index)
{
  unsigned short *ptr = row;
  ptr += index;
  return *ptr;
}

void func_800E23AC(patrolWaypointState *state);
typedef union 
{
  unsigned int raw;
  unsigned char b[4];
} targetMovementPoint;
typedef struct 
{
  unsigned int f0 : 1;
  unsigned int f1 : 1;
  unsigned int f2 : 1;
  unsigned int pad3 : 2;
  unsigned int f5 : 1;
  unsigned int pad6 : 26;
} targetMovementFlags;
typedef struct 
{
  char pad[9];
  unsigned char id;
} targetMovementActor;
typedef struct 
{
  char pad0[8];
  unsigned int flags;
  char padC[16];
  short x;
  char pad1E[2];
  short z;
} targetMovementModel;
typedef struct targetMovementState
{
  unsigned char b0;
  unsigned char b1;
  char pad2[1];
  unsigned char b3;
  unsigned char b4;
  char pad5[4];
  unsigned char b9;
  char padA[2];
  unsigned char bC;
  unsigned char bD;
  char padE[22];
  unsigned char b24;
  char pad25[14];
  unsigned char b33;
  vs_battle_movementPosition tile;
  char pad38[16];
  targetMovementPoint previousDestination;
  short x;
  char pad4E[2];
  short z;
  char pad52[2];
  targetMovementActor *actor;
  targetMovementModel *model;
  char pad5C[44];
  unsigned char id;
  char pad89[83];
  int range;
  int previousRange;
  unsigned char bE4;
  unsigned char targetId;
  unsigned char timeout;
  unsigned char bE7;
  unsigned char bE8;
  unsigned char rangeMode;
  unsigned char previousOption;
  unsigned char previousTargetId;
  targetMovementPoint cachedDestination;
  char padF0[4];
  unsigned char targetTileX;
  char padF5[1];
  unsigned char targetTileZ;
  char padF7[1];
  unsigned short timer;
  char padFA[94];
  short destinationX;
  short destinationZ;
  unsigned char movementTargetId;
  char pad15D[1];
  unsigned short flags15E;
  char pad160[8];
  targetMovementPoint destination;
  char pad16C[16];
  int targetX;
  int targetZ;
  char pad184[32];
  targetMovementFlags targets[16];
  unsigned int distances[16];
} targetMovementState;
int func_800E4FB0(void *, int, int);
void func_800E50A0(void *, int, int);
int func_800E4180(void *, int, int, int);
void func_800E4904(void *);
int func_800E24EC(targetMovementState *s, int option);
typedef struct 
{
  char prefix[0x4C];
  short x;
  short padding4E;
  short z;
  char padding52[6];
  D_800F4538_t *actor;
  char padding5C[0x2C];
  u_char actorId;
  char padding89[0x31];
  u_short delay;
  char paddingBC[0x4C];
  int previousDestination;
  u_char pending;
  char padding10D[0x2D];
  u_short timer;
  char padding13C[0x2C];
  int destination;
  char padding16C[0x10];
  int targetX;
  int targetZ;
  char padding184[4];
  u_int padding188 : 16;
  int angle : 16;
  char padding18C[0x18];
  u_int targetFlags[17];
  char padding1E8[0x299];
  u_char reaction;
} movementDecisionContext;
int func_800E4180(void *, int, int, int);
int func_800E2B2C(movementDecisionContext *context, int targetId);
typedef struct 
{
  char pad0[8];
  unsigned int flags;
  char padC[8];
  short angle14;
  char pad16[0x10];
  short angle26;
} movementRecoveryContext;
struct movementRecoveryState
{
  char pad0[9];
  unsigned char flag9;
  char padA[0x2A];
  vs_battle_movementPosition position;
  char pad38[0x14];
  short x;
  char pad4E[2];
  short z;
  char pad52[6];
  movementRecoveryContext *context;
  char pad5C[0xDE];
  unsigned short timer;
  unsigned short duration;
  char pad13E[0x1A];
  short targetX;
  short targetZ;
  unsigned char targetActor;
  char pad15D;
  unsigned short flags;
  char pad160[0x28];
  unsigned short facing188;
  unsigned short facing18A;
  char pad18C[0x9C];
  int counter;
};
typedef struct 
{
  char pad0[0x1C];
  short x;
  short y;
  short z;
} movementActorPosition;
void func_800E4B10(void *);
int func_800E4320(int, int);
void func_800E2CCC(movementRecoveryState *state);
typedef union 
{
  u_int raw;
  struct 
  {
    u_int unk0 : 1;
    u_int direction : 3;
    u_int clockwise : 1;
    u_int count : 7;
    u_int visited : 4;
    u_int stop : 1;
    u_int changed : 1;
    u_int cell : 4;
    u_int boundX : 5;
    u_int boundZ : 5;
  } bits;
} movementPath;
typedef struct 
{
  char unk0;
  u_char unk1;
  char unk2[2];
  u_char unk4;
  u_char unk5;
  char unk6[0x2E];
  vs_battle_movementPosition tile;
  char unk38[0x14];
  short x;
  char unk4E[2];
  short z;
  char unk52[0x92];
  u_char unkE4;
  char unkE5[2];
  u_char unkE7;
  u_char unkE8;
  char unkE9[0x75];
  u_short flags;
  char unk160[8];
  vs_battle_movementPosition destination;
  char unk16C[4];
  vs_battle_movementPosition next;
  char unk174[0x1C];
  movementPath path;
  char unk194[5];
  u_char unk199;
  u_char waitTimer;
} movementPathState;
typedef int (*movementPathTest)(movementPathState *, int);
int func_800D98E8(movementPathState *, int);
int func_800E3D5C(movementPathState *, movementPathTest);
int func_800E2F5C(movementPathState *state, movementPathTest test, int mask);
typedef struct 
{
  u_char reserved[4];
  u_char modeFlag;
  u_char reserved5[0x2F];
  vs_battle_movementPosition tile;
  u_char reserved38[0x51];
  u_char response;
  u_char reserved8A[0x28];
  u_char stepLimit;
  u_char reservedB3[0xA3];
  u_short cooldown;
  u_char reserved158[6];
  u_short flags;
  u_char reserved160[8];
  vs_battle_movementPosition destination;
  u_char reserved16C[4];
  vs_battle_movementPosition next;
  u_char reserved174[0x14];
  short facing;
  u_char reserved18A[6];
  movementPath path;
} movementControllerState;
typedef int (*movementRoute)(movementPathState *, int, int, movementPathTest);
typedef int (*movementStraightRoute)(movementPathState *, int, movementPathTest);
extern movementRoute D_800F1760[];
u_int func_800E49D8(void *, u_int, int, int);
int func_800D8400(void *, int, int, int);
int func_800E47E4(void *, u_int);
void func_800E3600(func_800E0850_t *input, int angle, int mode)
{
  movementControllerState *state = (movementControllerState *) input;
  movementPathTest test;
  vs_battle_movementPosition point;
  vs_battle_movementPosition probe;
  int mask;
  u_int quadrant;
  int direction;
  int firstDirection;
  int secondDirection;
  int diagonal;
  int firstOpen;
  int secondOpen;
  int distance;
  int maximumStep;
  int action;
  int aligned;
  int turn;
  if (((u_int) (mode - 2)) < 2)
  {
    test = (movementPathTest) func_800E0678;
    mask = 12;
  }
  else
  {
    test = (movementPathTest) D_800D9B6C;
    mask = 11;
  }
  if (state->path.raw & 1)
  {
    action = func_800E2F5C((movementPathState *) state, test, mask);
    if (action < 0)
    {
      func_800E4BB8((terrainMovementState *) state);
      goto recover;
    }
    goto pathDirection;
  }
  quadrant = (((u_int) angle) & 0xFFF) >> 10;
  direction = quadrant * 2;
  if ((((u_int) angle) & 0x3FF) > 512)
  {
    direction = ((quadrant + 1) & 3) * 2;
  }
  point = state->tile;
  point.p.x += ((D_800F16EC_t *) 0x1F8003EC)[direction].dx;
  point.p.z += ((D_800F16EC_t *) 0x1F8003EC)[direction].dz;
  if (direction & 2)
  {
    distance = state->destination.p.x - state->tile.p.x;
  }
  else
  {
    distance = state->destination.p.z - state->tile.p.z;
  }
  maximumStep = state->stepLimit;
  if (distance < 0)
  {
    distance = -distance;
  }
  if (maximumStep > distance)
  {
    maximumStep = distance;
  }
  point.raw = func_800E49D8(state, point.raw, direction, maximumStep);
  if (point.raw != 0)
  {
    point.p.y = 0;
    state->next = point;
    action = 14;
    goto setAction;
  }
  if ((angle & 0x3FF) == 0)
  {
    firstDirection = (((u_int) angle) >> 9) & 6;
    if (test((movementPathState *) state, firstDirection))
    {
      state->facing = angle;
      action = 7;
      goto setAction;
    }
    if (((u_int) (state->cooldown - 1)) >= 126)
    {
      probe.raw = func_800D8400(state, 0, 0, firstDirection);
      if (probe.p.status == 2)
      {
        if (state->cooldown == 0)
        {
          state->cooldown = 254;
        }
        goto recover;
      }
      if (probe.raw != 0)
      {
        if (func_800E47E4(state, probe.raw) && func_800E4180(state, 128, 2, state->response))
        {
          return;
        }
        state->next = probe;
        action = 14;
        goto setAction;
      }
    }
    if ((mode == 1) && (state->modeFlag == 0))
    {
      goto recover;
    }
    action = ((movementStraightRoute) D_800F1760[mode + 8])((movementPathState *) state, firstDirection, test);
    goto routeResult;
  }
  quadrant = ((u_int) angle) >> 10;
  firstDirection = ((quadrant + 1) * 2) & 7;
  secondDirection = (quadrant * 2) & 7;
  diagonal = ((quadrant * 2) + 1) & 7;
  firstOpen = test((movementPathState *) state, firstDirection);
  secondOpen = test((movementPathState *) state, secondDirection);
  if (!firstOpen)
  {
    if (secondOpen)
    {
      state->facing = secondDirection;
      goto oneDirection;
    }
    if (((u_int) (state->cooldown - 1)) >= 126)
    {
      probe.raw = func_800D8400(state, firstDirection, secondDirection, angle);
      if (probe.p.status == 2)
      {
        if (state->cooldown == 0)
        {
          state->cooldown = 254;
        }
        goto recover;
      }
      if (probe.raw != 0)
      {
        if (func_800E47E4(state, probe.raw) && func_800E4180(state, 128, 2, state->response))
        {
          return;
        }
        state->next = probe;
        action = 14;
        goto setAction;
      }
    }
    action = D_800F1760[mode]((movementPathState *) state, firstDirection, secondDirection, test);
    goto routeResult;
  }
  if (secondOpen)
  {
    if (!test((movementPathState *) state, diagonal))
    {
      if (direction == firstDirection)
      {
        state->facing = firstDirection;
      }
      else
      {
        state->facing = secondDirection;
      }
      action = 5;
    }
    else
    {
      state->facing = angle;
      action = 7;
    }
    goto setAction;
  }
  state->facing = firstDirection;
  oneDirection:
  if (((u_int) (state->cooldown - 1)) >= 126)
  {
    direction = firstDirection;
    if (firstOpen)
    {
      direction = secondDirection;
    }
    probe.raw = func_800D8400(state, 0, 0, direction);
    if (probe.p.status == 2)
    {
      if (state->cooldown == 0)
      {
        state->cooldown = 254;
      }
      goto recover;
    }
    if (probe.raw != 0)
    {
      if (func_800E47E4(state, probe.raw) && func_800E4180(state, 128, 2, state->response))
      {
        return;
      }
      state->next = probe;
      action = 14;
      goto setAction;
    }
  }

  aligned = 0;
  if ((mode == 0) || (mode == 2))
  {
    if (state->facing & 2)
    {
      if (state->tile.p.x == state->destination.p.x)
      {
        aligned = 1;
      }
    }
    else
      if (state->tile.p.z == state->destination.p.z)
    {
      aligned = 1;
    }
  }
  else
    if (state->modeFlag != 0)
  {
    direction = secondDirection;
    if (!firstOpen)
    {
      direction = firstDirection;
    }
    aligned = rcos(((direction * 512) - angle) & 0xFFF) > 3958;
  }
  turn = -1;
  if (!aligned)
  {
    action = 5;
    goto setAction;
  }
  direction = state->facing;
  if (firstOpen)
  {
    turn = 1;
  }
  action = D_800F1760[mode + 4]((movementPathState *) state, direction, turn, test);
  routeResult:
  if (action == 0)
  {
    goto recover;
  }

  action = 5;
  pathDirection:
  state->facing = (state->path.raw >> 1) & 7;

  setAction:
  func_800DEEFC((func_800E0850_t *) state, action);

  return;
  recover:
  func_800E2CCC((movementRecoveryState *) state);

}
