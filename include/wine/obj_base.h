/*
 * This file defines the macros and types necessary to define COM interfaces, 
 * and the three most basic COM interfaces: IUnknown, IMalloc and IClassFactory.
 */

#ifndef __WINE_WINE_OBJ_BASE_H
#define __WINE_WINE_OBJ_BASE_H

/*****************************************************************************
 * define ICOM_MSVTABLE_COMPAT
 * to implement the microsoft com vtable compatibility workaround for g++.
 *
 * NOTE: Turning this option on will produce a winelib that is incompatible
 * with the binary emulator.
 *
 * If the compiler supports the com_interface attribute, leave this off, and
 * define the ICOM_USE_COM_INTERFACE_ATTRIBUTE macro below.
 *
 * If you aren't interested in WineLib C++ compatability at all, leave both
 * options off.
 */
/* #define ICOM_MSVTABLE_COMPAT 1 */
/* #define ICOM_USE_COM_INTERFACE_ATTRIBUTE 1 */

/*****************************************************************************
 * Defines the basic types
 */
#include "wtypes.h"


#define LISet32(li, v)   ((li).HighPart = (v) < 0 ? -1 : 0, (li).LowPart = (v))
#define ULISet32(li, v)  ((li).HighPart = 0, (li).LowPart = (v))

/*****************************************************************************
 * Macros to declare the GUIDs
 */
#ifdef INITGUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        const GUID name = \
	{ l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
#else
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    extern const GUID name
#endif

#define DEFINE_OLEGUID(name, l, w1, w2) \
	DEFINE_GUID(name, l, w1, w2, 0xC0,0,0,0,0,0,0,0x46)

#define DEFINE_SHLGUID(name, l, w1, w2) DEFINE_OLEGUID(name,l,w1,w2)


/*****************************************************************************
 * GUID API
 */
HRESULT WINAPI StringFromCLSID16(REFCLSID id, LPOLESTR16*);
HRESULT WINAPI StringFromCLSID(REFCLSID id, LPOLESTR*);

HRESULT WINAPI CLSIDFromString16(LPCOLESTR16, CLSID *);
HRESULT WINAPI CLSIDFromString(LPCOLESTR, CLSID *);

HRESULT WINAPI CLSIDFromProgID16(LPCOLESTR16 progid, LPCLSID riid);
HRESULT WINAPI CLSIDFromProgID(LPCOLESTR progid, LPCLSID riid);

INT WINAPI StringFromGUID2(REFGUID id, LPOLESTR str, INT cmax);

BOOL16 WINAPI IsEqualGUID16(GUID* g1,GUID* g2);
BOOL WINAPI IsEqualGUID32(REFGUID rguid1,REFGUID rguid2);
/*#define IsEqualGUID WINELIB_NAME(IsEqualGUID)*/
#if defined(__cplusplus) && !defined(CINTERFACE)
#define IsEqualGUID(rguid1, rguid2) (!memcmp(&(rguid1), &(rguid2), sizeof(GUID)))
#else /* defined(__cplusplus) && !defined(CINTERFACE) */
#define IsEqualGUID(rguid1, rguid2) (!memcmp(rguid1, rguid2, sizeof(GUID)))
#endif /* defined(__cplusplus) && !defined(CINTERFACE) */
#define IsEqualIID(riid1, riid2) IsEqualGUID(riid1, riid2)
#define IsEqualCLSID(rclsid1, rclsid2) IsEqualGUID(rclsid1, rclsid2)

#if defined(__cplusplus) && !defined(CINTERFACE)
inline BOOL operator==(const GUID& guidOne, const GUID& guidOther)
{
    return !memcmp(&guidOne,&guidOther,sizeof(GUID));
}
inline BOOL operator!=(const GUID& guidOne, const GUID& guidOther)
{
    return !(guidOne == guidOther);
}
#endif 


/*****************************************************************************
 * Macros to define a COM interface
 */
/*
 * The goal of the following set of definitions is to provide a way to use the same 
 * header file definitions to provide both a C interface and a C++ object oriented 
 * interface to COM interfaces. The type of interface is selected automatically 
 * depending on the language but it is always possible to get the C interface in C++ 
 * by defining CINTERFACE.
 *
 * It is based on the following assumptions:
 *  - all COM interfaces derive from IUnknown, this should not be a problem.
 *  - the header file only defines the interface, the actual fields are defined 
 *    separately in the C file implementing the interface.
 *
 * The natural approach to this problem would be to make sure we get a C++ class and 
 * virtual methods in C++ and a structure with a table of pointer to functions in C.
 * Unfortunately the layout of the virtual table is compiler specific, the layout of 
 * g++ virtual tables is not the same as that of an egcs virtual table which is not the 
 * same as that generated by Visual C+. There are workarounds to make the virtual tables 
 * compatible via padding but unfortunately the one which is imposed to the WINE emulator
 * by the Windows binaries, i.e. the Visual C++ one, is the most compact of all.
 *
 * So the solution I finally adopted does not use virtual tables. Instead I use inline 
 * non virtual methods that dereference the method pointer themselves and perform the call.
 *
 * Let's take Direct3D as an example:
 *
 *    #define ICOM_INTERFACE IDirect3D
 *    #define IDirect3D_METHODS \
 *        ICOM_METHOD1(HRESULT,Initialize,    REFIID,) \
 *        ICOM_METHOD2(HRESULT,EnumDevices,   LPD3DENUMDEVICESCALLBACK,, LPVOID,) \
 *        ICOM_METHOD2(HRESULT,CreateLight,   LPDIRECT3DLIGHT*,, IUnknown*,) \
 *        ICOM_METHOD2(HRESULT,CreateMaterial,LPDIRECT3DMATERIAL*,, IUnknown*,) \
 *        ICOM_METHOD2(HRESULT,CreateViewport,LPDIRECT3DVIEWPORT*,, IUnknown*,) \
 *        ICOM_METHOD2(HRESULT,FindDevice,    LPD3DFINDDEVICESEARCH,, LPD3DFINDDEVICERESULT,)
 *    #define IDirect3D_IMETHODS \
 *        IUnknown_IMETHODS \
 *        IDirect3D_METHODS
 *    ICOM_DEFINE(IDirect3D,IUnknown)
 *    #undef ICOM_INTERFACE
 *
 *    #ifdef ICOM_CINTERFACE
 *    // *** IUnknown methods *** //
 *    #define IDirect3D_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
 *    #define IDirect3D_AddRef(p)             ICOM_CALL (AddRef,p)
 *    #define IDirect3D_Release(p)            ICOM_CALL (Release,p)
 *    // *** IDirect3D methods *** //
 *    #define IDirect3D_Initialize(p,a)       ICOM_CALL1(Initialize,p,a)
 *    #define IDirect3D_EnumDevices(p,a,b)    ICOM_CALL2(EnumDevice,p,a,b)
 *    #define IDirect3D_CreateLight(p,a,b)    ICOM_CALL2(CreateLight,p,a,b)
 *    #define IDirect3D_CreateMaterial(p,a,b) ICOM_CALL2(CreateMaterial,p,a,b)
 *    #define IDirect3D_CreateViewport(p,a,b) ICOM_CALL2(CreateViewport,p,a,b)
 *    #define IDirect3D_FindDevice(p,a,b)     ICOM_CALL2(FindDevice,p,a,b)
 *    #endif
 *
 * Comments:
 *  - The ICOM_INTERFACE macro is used in the ICOM_METHOD macros to define the type of the 'this' 
 *    pointer. Defining this macro here saves us the trouble of having to repeat the interface 
 *    name everywhere. Note however that because of the way macros work, a macro like ICOM_METHOD1 
 *    cannot use 'ICOM_INTERFACE##_VTABLE' because this would give 'ICOM_INTERFACE_VTABLE' and not 
 *    'IDirect3D_VTABLE'.
 *  - ICOM_METHODS defines the methods specific to this interface. It is then aggregated with the 
 *    inherited methods to form ICOM_IMETHODS.
 *  - ICOM_IMETHODS defines the list of methods that are inheritable from this interface. It must 
 *    be written manually (rather than using a macro to generate the equivalent code) to avoid 
 *    macro recursion (which compilers don't like).
 *  - The ICOM_DEFINE finally declares all the structures necessary for the interface. We have to 
 *    explicitly use the interface name for macro expansion reasons again.
 *    Inherited methods are inherited in C by using the IDirect3D_METHODS macro and the parent's 
 *    Xxx_IMETHODS macro. In C++ we need only use the IDirect3D_METHODS since method inheritance 
 *    is taken care of by the language.
 *  - In C++ the ICOM_METHOD macros generate a function prototype and a call to a function pointer 
 *    method. This means using once 't1 p1, t2 p2, ...' and once 'p1, p2' without the types. The 
 *    only way I found to handle this is to have one ICOM_METHOD macro per number of parameters and 
 *    to have it take only the type information (with const if necessary) as parameters.
 *    The 'undef ICOM_INTERFACE' is here to remind you that using ICOM_INTERFACE in the following 
 *    macros will not work. This time it's because the ICOM_CALL macro expansion is done only once 
 *    the 'IDirect3D_Xxx' macro is expanded. And by that time ICOM_INTERFACE will be long gone 
 *    anyway.
 *  - You may have noticed the double commas after each parameter type. This allows you to put the 
 *    name of that parameter which I think is good for documentation. It is not required and since 
 *    I did not know what to put there for this example (I could only find doc about IDirect3D2), 
 *    I left them blank.
 *  - Finally the set of 'IDirect3D_Xxx' macros is a standard set of macros defined to ease access 
 *    to the interface methods in C. Unfortunately I don't see any way to avoid having to duplicate 
 *    the inherited method definitions there. This time I could have used a trick to use only one 
 *    macro whatever the number of parameters but I prefered to have it work the same way as above.
 *  - You probably have noticed that we don't define the fields we need to actually implement this 
 *    interface: reference count, pointer to other resources and miscellaneous fields. That's 
 *    because these interfaces are just that: interfaces. They may be implemented more than once, in 
 *    different contexts and sometimes not even in Wine. Thus it would not make sense to impose 
 *    that the interface contains some specific fields.
 *
 *
 * In C this gives:
 *    typedef struct IDirect3DVtbl IDirect3DVtbl;
 *    struct IDirect3D {
 *        IDirect3DVtbl* lpvtbl;
 *    };
 *    struct IDirect3DVtbl {
 *        HRESULT (*fnQueryInterface)(IDirect3D* me, REFIID riid, LPVOID* ppvObj);
 *        ULONG (*fnQueryInterface)(IDirect3D* me);
 *        ULONG (*fnQueryInterface)(IDirect3D* me);
 *        HRESULT (*fnInitialize)(IDirect3D* me, REFIID a);
 *        HRESULT (*fnEnumDevices)(IDirect3D* me, LPD3DENUMDEVICESCALLBACK a, LPVOID b);
 *        HRESULT (*fnCreateLight)(IDirect3D* me, LPDIRECT3DLIGHT* a, IUnknown* b);
 *        HRESULT (*fnCreateMaterial)(IDirect3D* me, LPDIRECT3DMATERIAL* a, IUnknown* b);
 *        HRESULT (*fnCreateViewport)(IDirect3D* me, LPDIRECT3DVIEWPORT* a, IUnknown* b);
 *        HRESULT (*fnFindDevice)(IDirect3D* me, LPD3DFINDDEVICESEARCH a, LPD3DFINDDEVICERESULT b);
 *    }; 
 *
 *    #ifdef ICOM_CINTERFACE
 *    // *** IUnknown methods *** //
 *    #define IDirect3D_QueryInterface(p,a,b) (p)->lpvtbl->fnQueryInterface(p,a,b)
 *    #define IDirect3D_AddRef(p)             (p)->lpvtbl->fnAddRef(p)
 *    #define IDirect3D_Release(p)            (p)->lpvtbl->fnRelease(p)
 *    // *** IDirect3D methods *** //
 *    #define IDirect3D_Initialize(p,a)       (p)->lpvtbl->fnInitialize(p,a)
 *    #define IDirect3D_EnumDevices(p,a,b)    (p)->lpvtbl->fnEnumDevice(p,a,b)
 *    #define IDirect3D_CreateLight(p,a,b)    (p)->lpvtbl->fnCreateLight(p,a,b)
 *    #define IDirect3D_CreateMaterial(p,a,b) (p)->lpvtbl->fnCreateMaterial(p,a,b)
 *    #define IDirect3D_CreateViewport(p,a,b) (p)->lpvtbl->fnCreateViewport(p,a,b)
 *    #define IDirect3D_FindDevice(p,a,b)     (p)->lpvtbl->fnFindDevice(p,a,b)
 *    #endif
 *
 * Comments:
 *  - IDirect3D only contains a pointer to the IDirect3D virtual/jump table. This is the only thing 
 *    the user needs to know to use the interface. Of course the structure we will define to 
 *    implement this interface will have more fields but the first one will match this pointer.
 *  - The code generated by ICOM_DEFINE defines both the structure representing the interface and 
 *    the structure for the jump table. ICOM_DEFINE uses the parent's Xxx_IMETHODS macro to 
 *    automatically repeat the prototypes of all the inherited methods and then uses IDirect3D_METHODS 
 *    to define the IDirect3D methods.
 *  - Each method is declared as a pointer to function field in the jump table. The implementation 
 *    will fill this jump table with appropriate values, probably using a static variable, and 
 *    initialize the lpvtbl field to point to this variable.
 *  - The IDirect3D_Xxx macros then just derefence the lpvtbl pointer and use the function pointer 
 *    corresponding to the macro name. This emulates the behavior of a virtual table and should be 
 *    just as fast.
 *  - This C code should be quite compatible with the Windows headers both for code that uses COM 
 *    interfaces and for code implementing a COM interface.
 *
 *
 * And in C++ (with gcc's g++):
 *
 *    typedef struct IDirect3D: public IUnknown {
 *        private: HRESULT (*fnInitialize)(IDirect3D* me, REFIID a);
 *        public: inline HRESULT Initialize(REFIID a) { return ((IDirect3D*)t.lpvtbl)->fnInitialize(this,a); };
 *        private: HRESULT (*fnEnumDevices)(IDirect3D* me, LPD3DENUMDEVICESCALLBACK a, LPVOID b);
 *        public: inline HRESULT EnumDevices(LPD3DENUMDEVICESCALLBACK a, LPVOID b)
 *            { return ((IDirect3D*)t.lpvtbl)->fnEnumDevices(this,a,b); };
 *        private: HRESULT (*fnCreateLight)(IDirect3D* me, LPDIRECT3DLIGHT* a, IUnknown* b);
 *        public: inline HRESULT CreateLight(LPDIRECT3DLIGHT* a, IUnknown* b)
 *            { return ((IDirect3D*)t.lpvtbl)->fnCreateLight(this,a,b); };
 *        private: HRESULT (*fnCreateMaterial)(IDirect3D* me, LPDIRECT3DMATERIAL* a, IUnknown* b);
 *        public: inline HRESULT CreateMaterial(LPDIRECT3DMATERIAL* a, IUnknown* b)
 *            { return ((IDirect3D*)t.lpvtbl)->fnCreateMaterial(this,a,b); };
 *        private: HRESULT (*fnCreateViewport)(IDirect3D* me, LPDIRECT3DVIEWPORT* a, IUnknown* b);
 *        public: inline HRESULT CreateViewport(LPDIRECT3DVIEWPORT* a, IUnknown* b)
 *            { return ((IDirect3D*)t.lpvtbl)->fnCreateViewport(this,a,b); };
 *        private:  HRESULT (*fnFindDevice)(IDirect3D* me, LPD3DFINDDEVICESEARCH a, LPD3DFINDDEVICERESULT b);
 *        public: inline HRESULT FindDevice(LPD3DFINDDEVICESEARCH a, LPD3DFINDDEVICERESULT b)
 *            { return ((IDirect3D*)t.lpvtbl)->fnFindDevice(this,a,b); };
 *    }; 
 *
 * Comments:
 *  - In C++ IDirect3D does double duty as both the virtual/jump table and as the interface 
 *    definition. The reason for this is to avoid having to duplicate the mehod definitions: once 
 *    to have the function pointers in the jump table and once to have the methods in the interface 
 *    class. Here one macro can generate both. This means though that the first pointer, t.lpvtbl 
 *    defined in IUnknown,  must be interpreted as the jump table pointer if we interpret the 
 *    structure as the the interface class, and as the function pointer to the QueryInterface 
 *    method, t.fnQueryInterface, if we interpret the structure as the jump table. Fortunately this 
 *    gymnastic is entirely taken care of in the header of IUnknown.
 *  - Of course in C++ we use inheritance so that we don't have to duplicate the method definitions. 
 *  - Since IDirect3D does double duty, each ICOM_METHOD macro defines both a function pointer and 
 *    a non-vritual inline method which dereferences it and calls it. This way this method behaves 
 *    just like a virtual method but does not create a true C++ virtual table which would break the 
 *    structure layout. If you look at the implementation of these methods you'll notice that they 
 *    would not work for void functions. We have to return something and fortunately this seems to 
 *    be what all the COM methods do (otherwise we would need another set of macros).
 *  - Note how the ICOM_METHOD generates both function prototypes mixing types and formal parameter 
 *    names and the method invocation using only the formal parameter name. This is the reason why 
 *    we need different macros to handle different numbers of parameters.
 *  - Finally there is no IDirect3D_Xxx macro. These are not needed in C++ unless the CINTERFACE 
 *    macro is defined in which case we would not be here.
 *  - This C++ code works well for code that just uses COM interfaces. But it will not work with 
 *    C++ code implement a COM interface. That's because such code assumes the interface methods 
 *    are declared as virtual C++ methods which is not the case here.
 *
 *
 * Implementing a COM interface.
 *
 * This continues the above example. This example assumes that the implementation is in C.
 *
 *    typedef struct _IDirect3D {
 *        void* lpvtbl;
 *        // ...
 *
 *    } _IDirect3D;
 *
 *    static ICOM_VTABLE(IDirect3D) d3dvt;
 *
 *    // implement the IDirect3D methods here
 *
 *    int IDirect3D_fnQueryInterface(IDirect3D* me)
 *    {
 *        ICOM_THIS(IDirect3D,me);
 *        // ...
 *    }
 *
 *    // ...
 *
 *    static ICOM_VTABLE(IDirect3D) d3dvt = {
 *            IDirect3D_fnQueryInterface,
 *        IDirect3D_fnAdd,
 *        IDirect3D_fnAdd2,
 *        IDirect3D_fnInitialize,
 *        IDirect3D_fnSetWidth
 *    };
 *
 * Comments:
 *  - We first define what the interface really contains. This is th e_IDirect3D structure. The 
 *    first field must of course be the virtual table pointer. Everything else is free.
 *  - Then we predeclare our static virtual table variable, we will need its address in some 
 *    methods to initialize the virtual table pointer of the returned interface objects.
 *  - Then we implement the interface methods. To match what has been declared in the header file 
 *    they must take a pointer to a IDirect3D structure and we must cast it to an _IDirect3D so that 
 *    we can manipulate the fields. This is performed by the ICOM_THIS macro.
 *  - Finally we initialize the virtual table.
 */


#define ICOM_VTABLE(iface)       iface##Vtbl


#if !defined(__cplusplus) || defined(CINTERFACE)
#define ICOM_CINTERFACE 1
#endif

#ifndef ICOM_CINTERFACE
/* C++ interface */

#define ICOM_METHOD(ret,xfn) \
     public: virtual ret (CALLBACK xfn)(void) = 0;

#define ICOM_METHOD1(ret,xfn,ta,na) \
     public: virtual ret (CALLBACK xfn)(ta a) = 0;

#define ICOM_METHOD2(ret,xfn,ta,na,tb,nb) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b) = 0;

#define ICOM_METHOD3(ret,xfn,ta,na,tb,nb,tc,nc) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c) = 0;

#define ICOM_METHOD4(ret,xfn,ta,na,tb,nb,tc,nc,td,nd) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d) = 0;

#define ICOM_METHOD5(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e) = 0;

#define ICOM_METHOD6(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f) = 0;

#define ICOM_METHOD7(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g) = 0;

#define ICOM_METHOD8(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g,th h) = 0;

#define ICOM_METHOD9(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh,ti,ni) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g,th h,ti i) = 0;

#define ICOM_METHOD10(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh,ti,ni,tj,nj) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g,th h,ti i,tj j) = 0;


#define ICOM_CMETHOD(ret,xfn) \
     public: virtual ret (CALLBACK xfn)(void) const = 0;

#define ICOM_CMETHOD1(ret,xfn,ta,na) \
     public: virtual ret (CALLBACK xfn)(ta a) const = 0;

#define ICOM_CMETHOD2(ret,xfn,ta,na,tb,nb) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b) const = 0;

#define ICOM_CMETHOD3(ret,xfn,ta,na,tb,nb,tc,nc) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c) const = 0;

#define ICOM_CMETHOD4(ret,xfn,ta,na,tb,nb,tc,nc,td,nd) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d) const = 0;

#define ICOM_CMETHOD5(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e) const = 0;

#define ICOM_CMETHOD6(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f) const = 0;

#define ICOM_CMETHOD7(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g) const = 0;

#define ICOM_CMETHOD8(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh) \
     public: virtual ret (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g,th h) const = 0;


#define ICOM_VMETHOD(xfn) \
     public: virtual void (CALLBACK xfn)(void) = 0;

#define ICOM_VMETHOD1(xfn,ta,na) \
     public: virtual void (CALLBACK xfn)(ta a) = 0;

#define ICOM_VMETHOD2(xfn,ta,na,tb,nb) \
     public: virtual void (CALLBACK xfn)(ta a,tb b) = 0;

#define ICOM_VMETHOD3(xfn,ta,na,tb,nb,tc,nc) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c) = 0;

#define ICOM_VMETHOD4(xfn,ta,na,tb,nb,tc,nc,td,nd) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d) = 0;

#define ICOM_VMETHOD5(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d,te e) = 0;

#define ICOM_VMETHOD6(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f) = 0;

#define ICOM_VMETHOD7(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g) = 0;

#define ICOM_VMETHOD8(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g,th h) = 0;


#define ICOM_CVMETHOD(xfn) \
     public: virtual void (CALLBACK xfn)(void) const = 0;

#define ICOM_CVMETHOD1(xfn,ta,na) \
     public: virtual void (CALLBACK xfn)(ta a) const = 0;

#define ICOM_CVMETHOD2(xfn,ta,na,tb,nb) \
     public: virtual void (CALLBACK xfn)(ta a,tb b) const = 0;

#define ICOM_CVMETHOD3(xfn,ta,na,tb,nb,tc,nc) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c) const = 0;

#define ICOM_CVMETHOD4(xfn,ta,na,tb,nb,tc,nc,td,nd) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d) const = 0;

#define ICOM_CVMETHOD5(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d,te e) const = 0;

#define ICOM_CVMETHOD6(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f) const = 0;

#define ICOM_CVMETHOD7(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g) const = 0;

#define ICOM_CVMETHOD8(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh) \
     public: virtual void (CALLBACK xfn)(ta a,tb b,tc c,td d,te e,tf f,tg g,th h) const = 0;

#ifdef ICOM_USE_COM_INTERFACE_ATTRIBUTE

#define ICOM_DEFINE(iface,ibase) \
    typedef struct iface: public ibase { \
        iface##_METHODS \
            } __attribute__ ((com_interface));

#else

#define ICOM_DEFINE(iface,ibase) \
    typedef struct iface: public ibase { \
        iface##_METHODS \
    };

#endif /* ICOM_USE_COM_INTERFACE_ATTRIBUTE */

#define ICOM_CALL(xfn, p)                        this_is_a_syntax_error
#define ICOM_CALL1(xfn, p,a)                     this_is_a_syntax_error
#define ICOM_CALL2(xfn, p,a,b)                   this_is_a_syntax_error
#define ICOM_CALL3(xfn, p,a,b,c)                 this_is_a_syntax_error
#define ICOM_CALL4(xfn, p,a,b,c,d)               this_is_a_syntax_error
#define ICOM_CALL5(xfn, p,a,b,c,d,e)             this_is_a_syntax_error
#define ICOM_CALL6(xfn, p,a,b,c,d,e,f)           this_is_a_syntax_error
#define ICOM_CALL7(xfn, p,a,b,c,d,e,f,g)         this_is_a_syntax_error
#define ICOM_CALL8(xfn, p,a,b,c,d,e,f,g,h) this_is_a_syntax_error


#else
/* C interface */


#define ICOM_METHOD(ret,xfn) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me);

#define ICOM_METHOD1(ret,xfn,ta,na) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a);

#define ICOM_METHOD2(ret,xfn,ta,na,tb,nb) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b);

#define ICOM_METHOD3(ret,xfn,ta,na,tb,nb,tc,nc) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c);

#define ICOM_METHOD4(ret,xfn,ta,na,tb,nb,tc,nc,td,nd) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d);

#define ICOM_METHOD5(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e);

#define ICOM_METHOD6(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f);

#define ICOM_METHOD7(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g);

#define ICOM_METHOD8(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g,th h);

#define ICOM_METHOD9(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh,ti,ni) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g,th h,ti i);

#define ICOM_METHOD10(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh,ti,ni,tj,nj) \
    ret (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g,th h,ti i,tj j);


#define ICOM_CMETHOD(ret,xfn) \
        ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me);

#define ICOM_CMETHOD1(ret,xfn,ta,na) \
    ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a);

#define ICOM_CMETHOD2(ret,xfn,ta,na,tb,nb) \
    ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b);

#define ICOM_CMETHOD3(ret,xfn,ta,na,tb,nb,tc,nc) \
    ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c);

#define ICOM_CMETHOD4(ret,xfn,ta,na,tb,nb,tc,nc,td,nd) \
    ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d);

#define ICOM_CMETHOD5(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne) \
    ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e);

#define ICOM_CMETHOD6(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf) \
    ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f);

#define ICOM_CMETHOD7(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng) \
    ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g);

#define ICOM_CMETHOD8(ret,xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh) \
    ret (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g,th h);


#define ICOM_VMETHOD(xfn) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me);

#define ICOM_VMETHOD1(xfn,ta,na) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a);

#define ICOM_VMETHOD2(xfn,ta,na,tb,nb) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b);

#define ICOM_VMETHOD3(xfn,ta,na,tb,nb,tc,nc) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c);

#define ICOM_VMETHOD4(xfn,ta,na,tb,nb,tc,nc,td,nd) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d);

#define ICOM_VMETHOD5(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e);

#define ICOM_VMETHOD6(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f);

#define ICOM_VMETHOD7(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g);

#define ICOM_VMETHOD8(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,nh) \
    void (CALLBACK *fn##xfn)(ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g,th h);


#define ICOM_CVMETHOD(xfn) \
        void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me);

#define ICOM_CVMETHOD1(xfn,ta,na) \
    void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a);

#define ICOM_CVMETHOD2(xfn,ta,na,tb,nb) \
    void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b);

#define ICOM_CVMETHOD3(xfn,ta,na,tb,nb,tc,nc) \
    void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c);

#define ICOM_CVMETHOD4(xfn,ta,na,tb,nb,tc,nc,td,nd) \
    void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d);

#define ICOM_CVMETHOD5(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne) \
    void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e);

#define ICOM_CVMETHOD6(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf) \
    void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f);

#define ICOM_CVMETHOD7(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng) \
    void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g);

#define ICOM_CVMETHOD8(xfn,ta,na,tb,nb,tc,nc,td,nd,te,ne,tf,nf,tg,ng,th,nh) \
    void (CALLBACK *fn##xfn)(const ICOM_INTERFACE* me,ta a,tb b,tc c,td d,te e,tf f,tg g,th h);


#ifdef ICOM_MSVTABLE_COMPAT
#define ICOM_DEFINE(iface,ibase) \
    typedef struct ICOM_VTABLE(iface) ICOM_VTABLE(iface); \
    struct iface { \
        const ICOM_VTABLE(iface)* lpvtbl; \
    }; \
    struct ICOM_VTABLE(iface) { \
        long dummyRTTI1; \
        long dummyRTTI2; \
        ibase##_IMETHODS \
        iface##_METHODS \
    };
#define ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE 0,0,

#else
#define ICOM_DEFINE(iface,ibase) \
    typedef struct ICOM_VTABLE(iface) ICOM_VTABLE(iface); \
    struct iface { \
        const ICOM_VTABLE(iface)* lpvtbl; \
    }; \
    struct ICOM_VTABLE(iface) { \
        ibase##_IMETHODS \
        iface##_METHODS \
    };
#define ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
#endif /* ICOM_MSVTABLE_COMPAT */


#define ICOM_CALL(xfn, p)  (p)->lpvtbl->fn##xfn(p)
#define ICOM_CALL1(xfn, p,a) (p)->lpvtbl->fn##xfn(p,a)
#define ICOM_CALL2(xfn, p,a,b) (p)->lpvtbl->fn##xfn(p,a,b)
#define ICOM_CALL3(xfn, p,a,b,c) (p)->lpvtbl->fn##xfn(p,a,b,c)
#define ICOM_CALL4(xfn, p,a,b,c,d) (p)->lpvtbl->fn##xfn(p,a,b,c,d)
#define ICOM_CALL5(xfn, p,a,b,c,d,e) (p)->lpvtbl->fn##xfn(p,a,b,c,d,e)
#define ICOM_CALL6(xfn, p,a,b,c,d,e,f) (p)->lpvtbl->fn##xfn(p,a,b,c,d,e,f)
#define ICOM_CALL7(xfn, p,a,b,c,d,e,f,g) (p)->lpvtbl->fn##xfn(p,a,b,c,d,e,f,g)
#define ICOM_CALL8(xfn, p,a,b,c,d,e,f,g,h) (p)->lpvtbl->fn##xfn(p,a,b,c,d,e,f,g,h)
#define ICOM_CALL9(xfn, p,a,b,c,d,e,f,g,h,i) (p)->lpvtbl->fn##xfn(p,a,b,c,d,e,f,g,h,i)
#define ICOM_CALL10(xfn, p,a,b,c,d,e,f,g,h,i,j) (p)->lpvtbl->fn##xfn(p,a,b,c,d,e,f,g,h,i,j)


#define ICOM_THIS(impl,iface)          impl* const This=(impl*)iface
#define ICOM_CTHIS(impl,iface)         const impl* const This=(const impl*)iface

#endif


/*****************************************************************************
 * Predeclare the interfaces
 */
DEFINE_OLEGUID(IID_IClassFactory,	0x00000001L, 0, 0);
typedef struct IClassFactory IClassFactory, *LPCLASSFACTORY;

DEFINE_OLEGUID(IID_IMalloc,		0x00000002L, 0, 0);
typedef struct IMalloc16 IMalloc16,*LPMALLOC16;
typedef struct IMalloc IMalloc,*LPMALLOC;

DEFINE_OLEGUID(IID_IUnknown,		0x00000000L, 0, 0);
typedef struct IUnknown IUnknown, *LPUNKNOWN;


/*****************************************************************************
 * IUnknown interface
 */
#define ICOM_INTERFACE IUnknown
#define IUnknown_IMETHODS \
    ICOM_METHOD2(HRESULT,QueryInterface,REFIID,riid, LPVOID*,ppvObj) \
    ICOM_METHOD (ULONG,AddRef) \
    ICOM_METHOD (ULONG,Release)
#ifdef ICOM_CINTERFACE
typedef struct ICOM_VTABLE(IUnknown) ICOM_VTABLE(IUnknown);
struct IUnknown {
    ICOM_VTABLE(IUnknown)* lpvtbl;
#if defined(ICOM_USE_COM_INTERFACE_ATTRIBUTE) && !defined(ICOM_CINTERFACE)
} __attribute__ ((com_interface)); 
#else
};
#endif /* ICOM_US_COM_INTERFACE_ATTRIBUTE, !ICOM_CINTERFACE */

struct ICOM_VTABLE(IUnknown) {
#ifdef ICOM_MSVTABLE_COMPAT
    long dummyRTTI1;
    long dummyRTTI2;
#endif /* ICOM_MSVTABLE_COMPAT */

#else /* ICOM_CINTERFACE */
struct IUnknown {

#endif /* ICOM_CINTERFACE */

    ICOM_METHOD2(HRESULT,QueryInterface,REFIID,riid, LPVOID*,ppvObj)
    ICOM_METHOD (ULONG,AddRef)
    ICOM_METHOD (ULONG,Release)
};
#undef ICOM_INTERFACE

#ifdef ICOM_CINTERFACE
/*** IUnknown methods ***/
#define IUnknown_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IUnknown_AddRef(p)             ICOM_CALL (AddRef,p)
#define IUnknown_Release(p)            ICOM_CALL (Release,p)
#endif

/*****************************************************************************
 * IClassFactory interface
 */
#define ICOM_INTERFACE IClassFactory
#define IClassFactory_METHODS \
    ICOM_METHOD3(HRESULT,CreateInstance, LPUNKNOWN,pUnkOuter, REFIID,riid, LPVOID*,ppvObject) \
    ICOM_METHOD1(HRESULT,LockServer,     BOOL,fLock)
#define IClassFactory_IMETHODS \
    IUnknown_IMETHODS \
    IClassFactory_METHODS
ICOM_DEFINE(IClassFactory,IUnknown)
#undef ICOM_INTERFACE

#ifdef ICOM_CINTERFACE
/*** IUnknown methods ***/
#define IClassFactory_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IClassFactory_AddRef(p)             ICOM_CALL (AddRef,p)
#define IClassFactory_Release(p)            ICOM_CALL (Release,p)
/*** IClassFactory methods ***/
#define IClassFactory_CreateInstance(p,a,b,c) ICOM_CALL3(CreateInstance,p,a,b,c)
#define IClassFactory_LockServer(p,a)         ICOM_CALL1(LockServer,p,a)
#endif


/*****************************************************************************
 * IMalloc interface
 */
#define ICOM_INTERFACE IMalloc16
#define IMalloc16_METHODS \
    ICOM_METHOD1 (LPVOID,Alloc,       DWORD,cb) \
    ICOM_METHOD2 (LPVOID,Realloc,     LPVOID,pv, DWORD,cb) \
    ICOM_VMETHOD1(       Free,        LPVOID,pv) \
    ICOM_CMETHOD1(DWORD, GetSize,     LPVOID,pv) \
    ICOM_CMETHOD1(INT16, DidAlloc,    LPVOID,pv) \
    ICOM_METHOD  (LPVOID,HeapMinimize)
#define IMalloc16_IMETHODS \
    IUnknown_IMETHODS \
    IMalloc16_METHODS
ICOM_DEFINE(IMalloc16,IUnknown)
#undef ICOM_INTERFACE

#ifdef ICOM_CINTERFACE
/*** IUnknown methods ***/
#define IMalloc16_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IMalloc16_AddRef(p)             ICOM_CALL (AddRef,p)
#define IMalloc16_Release(p)            ICOM_CALL (Release,p)
/*** IMalloc16 methods ***/
#define IMalloc16_Alloc(p,a)      ICOM_CALL1(Alloc,p,a)
#define IMalloc16_Realloc(p,a,b)  ICOM_CALL2(Realloc,p,a,b)
#define IMalloc16_Free(p,a)       ICOM_CALL1(Free,p,a)
#define IMalloc16_GetSize(p,a)    ICOM_CALL1(GetSize,p,a)
#define IMalloc16_DidAlloc(p,a)   ICOM_CALL1(DidAlloc,p,a)
#define IMalloc16_HeapMinimize(p) ICOM_CALL (HeapMinimize,p)
#endif


#define ICOM_INTERFACE IMalloc
#define IMalloc_METHODS \
    ICOM_METHOD1 (LPVOID,Alloc,       DWORD,cb) \
    ICOM_METHOD2 (LPVOID,Realloc,     LPVOID,pv, DWORD,cb) \
    ICOM_VMETHOD1(       Free,        LPVOID,pv) \
    ICOM_CMETHOD1(DWORD, GetSize,     LPVOID,pv) \
    ICOM_CMETHOD1(INT, DidAlloc,    LPVOID,pv) \
    ICOM_METHOD  (LPVOID,HeapMinimize)
#define IMalloc_IMETHODS \
    IUnknown_IMETHODS \
    IMalloc_METHODS
ICOM_DEFINE(IMalloc,IUnknown)
#undef ICOM_INTERFACE

#ifdef ICOM_CINTERFACE
/*** IUnknown methods ***/
#define IMalloc_QueryInterface(p,a,b) ICOM_CALL2(QueryInterface,p,a,b)
#define IMalloc_AddRef(p)             ICOM_CALL (AddRef,p)
#define IMalloc_Release(p)            ICOM_CALL (Release,p)
/*** IMalloc32 methods ***/
#define IMalloc_Alloc(p,a)      ICOM_CALL1(Alloc,p,a)
#define IMalloc_Realloc(p,a,b)  ICOM_CALL2(Realloc,p,a,b)
#define IMalloc_Free(p,a)       ICOM_CALL1(Free,p,a)
#define IMalloc_GetSize(p,a)    ICOM_CALL1(GetSize,p,a)
#define IMalloc_DidAlloc(p,a)   ICOM_CALL1(DidAlloc,p,a)
#define IMalloc_HeapMinimize(p) ICOM_CALL (HeapMinimize,p)
#endif


HRESULT WINAPI CoCreateStandardMalloc16(DWORD dwMemContext, LPMALLOC16* lpMalloc);

HRESULT WINAPI CoGetMalloc16(DWORD dwMemContext,LPMALLOC16* lpMalloc);
HRESULT WINAPI CoGetMalloc(DWORD dwMemContext,LPMALLOC* lpMalloc);

LPVOID WINAPI CoTaskMemAlloc(ULONG size);

void WINAPI CoTaskMemFree(LPVOID ptr);

/* FIXME: unimplemented */
LPVOID WINAPI CoTaskMemRealloc(LPVOID ptr, ULONG size);


/*****************************************************************************
 * Additional API
 */

HRESULT WINAPI CoCreateGuid(GUID* pguid);

void WINAPI CoFreeAllLibraries(void);

void WINAPI CoFreeLibrary(HINSTANCE hLibrary);

void WINAPI CoFreeUnusedLibraries(void);

HRESULT WINAPI CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID iid, LPVOID *ppv);

HRESULT WINAPI CoGetClassObject(REFCLSID rclsid, DWORD dwClsContext, LPVOID pvReserved, REFIID iid, LPVOID *ppv);

HRESULT WINAPI CoInitialize16(LPVOID lpReserved);
HRESULT WINAPI CoInitialize(LPVOID lpReserved);
HRESULT WINAPI CoInitializeEx(LPVOID lpReserved, DWORD dwCoInit);

void WINAPI CoUninitialize16(void);
void WINAPI CoUninitialize(void);

typedef enum tagCOINIT
{
    COINIT_APARTMENTTHREADED  = 0x2, /* Apartment model */
    COINIT_MULTITHREADED      = 0x0, /* OLE calls objects on any thread */
    COINIT_DISABLE_OLE1DDE    = 0x4, /* Don't use DDE for Ole1 support */
    COINIT_SPEED_OVER_MEMORY  = 0x8  /* Trade memory for speed */
} COINIT;


/* FIXME: not implemented */
BOOL WINAPI CoIsOle1Class(REFCLSID rclsid);

HINSTANCE WINAPI CoLoadLibrary(LPOLESTR16 lpszLibName, BOOL bAutoFree);

HRESULT WINAPI CoLockObjectExternal16(LPUNKNOWN pUnk, BOOL16 fLock, BOOL16 fLastUnlockReleases);
HRESULT WINAPI CoLockObjectExternal(LPUNKNOWN pUnk, BOOL fLock, BOOL fLastUnlockReleases);

/* class registration flags; passed to CoRegisterClassObject */
typedef enum tagREGCLS
{
    REGCLS_SINGLEUSE = 0,
    REGCLS_MULTIPLEUSE = 1,
    REGCLS_MULTI_SEPARATE = 2,
    REGCLS_SUSPENDED = 4
} REGCLS;

HRESULT WINAPI CoRegisterClassObject16(REFCLSID rclsid, LPUNKNOWN pUnk, DWORD dwClsContext, DWORD flags, LPDWORD lpdwRegister);
HRESULT WINAPI CoRegisterClassObject(REFCLSID rclsid,LPUNKNOWN pUnk,DWORD dwClsContext,DWORD flags,LPDWORD lpdwRegister);

HRESULT WINAPI CoRevokeClassObject(DWORD dwRegister);

void WINAPI CoUninitialize16(void);
void WINAPI CoUninitialize(void);

/*****************************************************************************
 *	COM Server dll - exports
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID * ppv);
HRESULT WINAPI DllCanUnloadNow(void);

/*****************************************************************************
 * Internal WINE API
 */
#ifdef __WINE__
HRESULT WINE_StringFromCLSID(const CLSID *id, LPSTR);
#endif

#endif /* __WINE_WINE_OBJ_BASE_H */
