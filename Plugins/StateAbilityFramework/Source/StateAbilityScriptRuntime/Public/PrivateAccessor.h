#pragma once

#define PRIVATE_DEFINE(Class, Type, Member) struct FExternal_##Member {\
        using type = Type Class::*;\
        };\
        template struct FPtrTaker<FExternal_##Member, &Class::Member>;
#define PRIVATE_GET(Obj, Member) Obj->*FAccess<FExternal_##Member>::ptr

#define PRIVATE_DECLARE_NAMESPACE(Class, Type, Member) struct FExternal_##Member {\
        using type = Type Class::*;\
        };

#define PRIVATE_DECLARE_FUNC_NAMESPACE(Class, ReturnType, Member, ...) struct FExternal_Func_##Member {\
        using type = ReturnType(Class::*)(__VA_ARGS__);\
        };


#define PRIVATE_DEFINE_NAMESPACE(Namespace, Class, Member) template struct FPtrTaker<Namespace::FExternal_##Member, &Class::Member>;
#define PRIVATE_DEFINE_FUNC_NAMESPACE(Namespace, Class, ReturnType, Member, ...) template struct FPtrTaker<Namespace::FExternal_Func_##Member, &Class::Member>;

#define PRIVATE_GET_NAMESPACE(Namespace, Obj, Member) Obj->*FAccess<Namespace::FExternal_##Member>::ptr
#define PRIVATE_GET_FUNC_NAMESPACE(Namespace, Obj, Member) (Obj->*FAccess<Namespace::FExternal_Func_##Member>::ptr)

template <typename Tag>
class FAccess {
public:
	inline static typename Tag::type ptr;
};

template <typename Tag, typename Tag::type V>
struct FPtrTaker {
	struct FTransferer {
		FTransferer() {
			FAccess<Tag>::ptr = V;
		}
	};
	inline static FTransferer tr;
};