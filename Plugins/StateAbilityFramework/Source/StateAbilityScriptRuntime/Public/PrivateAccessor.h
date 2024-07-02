#pragma once

#define PRIVATE_DEFINE(Class, Type, Member) struct FExternal_##Member {\
        using type = Type Class::*;\
        };\
        template struct FPtrTaker<FExternal_##Member, &Class::Member>;
#define PRIVATE_GET(Obj, Member) Obj->*FAccess<FExternal_##Member>::ptr

#define PRIVATE_DECLARE_NAMESPACE(Class, Type, Member) struct FExternal_##Member {\
        using type = Type Class::*;\
        };
#define PRIVATE_DEFINE_NAMESPACE(Class, Member, Namespace) template struct FPtrTaker<Namespace::FExternal_##Member, &Class::Member>;
#define PRIVATE_GET_NAMESPACE(Obj, Member, Namespace) Obj->*FAccess<Namespace::FExternal_##Member>::ptr

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