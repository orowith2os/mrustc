/*
 * MRustC - Rust Compiler
 * - By John Hodge (Mutabah/thePowersGang)
 *
 * expand/derive.cpp
 * - Support for the `#[derive]` attribute
 */
#include <synext.hpp>
#include "../common.hpp"
#include "../ast/ast.hpp"
#include "../ast/expr.hpp"
#include "../ast/crate.hpp"
#include <hir/hir.hpp>  // ABI_RUST
#include <parse/common.hpp>    // Parse_ModRoot_Items
#include "proc_macro.hpp"

template<typename T>
static inline ::std::vector<T> vec$(T v1) {
    ::std::vector<T> tmp;
    tmp.push_back( mv$(v1) );
    return mv$(tmp);
}
template<typename T>
static inline ::std::vector<T> vec$(T v1, T v2) {
    ::std::vector<T> tmp;
    tmp.reserve(2);
    tmp.push_back( mv$(v1) );
    tmp.push_back( mv$(v2) );
    return mv$(tmp);
}
template<typename T>
static inline ::std::vector<T> vec$(T v1, T v2, T v3) {
    ::std::vector<T> tmp;
    tmp.reserve(3);
    tmp.push_back( mv$(v1) );
    tmp.push_back( mv$(v2) );
    tmp.push_back( mv$(v3) );
    return mv$(tmp);
}
template<typename T>
static inline ::std::vector<T> vec$(T v1, T v2, T v3, T v4) {
    ::std::vector<T> tmp;
    tmp.reserve(4);
    tmp.push_back( mv$(v1) );
    tmp.push_back( mv$(v2) );
    tmp.push_back( mv$(v3) );
    tmp.push_back( mv$(v4) );
    return mv$(tmp);
}
template<typename T>
static inline ::std::vector<T> vec$(T v1, T v2, T v3, T v4, T v5) {
    ::std::vector<T> tmp;
    tmp.reserve(5);
    tmp.push_back( mv$(v1) );
    tmp.push_back( mv$(v2) );
    tmp.push_back( mv$(v3) );
    tmp.push_back( mv$(v4) );
    tmp.push_back( mv$(v5) );
    return mv$(tmp);
}
static AST::Path get_path(const RcString& core_name, const char* c1, const char* c2)
{
    return AST::Path(core_name, { AST::PathNode(RcString::new_interned(c1), {}), AST::PathNode(RcString::new_interned(c2), {}) });
}
static AST::Path get_path(const RcString& core_name, const char* c1, const char* c2, const char* c3)
{
    return AST::Path(core_name, { AST::PathNode(RcString::new_interned(c1), {}), AST::PathNode(RcString::new_interned(c2), {}), AST::PathNode(RcString::new_interned(c3), {}) });
}

static inline AST::ExprNodeP mk_exprnodep(AST::ExprNode* en){ return AST::ExprNodeP(en); }
//#define NEWNODE(type, ...)  mk_exprnodep(new type(__VA_ARGS__))
#define NEWNODE(type, ...)  mk_exprnodep(new AST::ExprNode_##type(__VA_ARGS__))

static ::std::vector<AST::ExprNodeP> make_refpat_a(
        const Span& sp,
        ::std::vector<AST::Pattern>&    pats_a,
        const ::std::vector<TypeRef>&   sub_types,
        ::std::function<AST::ExprNodeP(size_t, AST::ExprNodeP)> cb
    )
{
    ::std::vector<AST::ExprNodeP>   nodes;
    for( size_t idx = 0; idx < sub_types.size(); idx ++ )
    {
        auto name_a = RcString::new_interned(FMT("a" << idx));
        pats_a.push_back( ::AST::Pattern(::AST::Pattern::TagBind(), sp, name_a, ::AST::PatternBinding::Type::REF) );
        nodes.push_back( cb(idx, NEWNODE(NamedValue, AST::Path(name_a))) );
    }
    return nodes;
}
static ::std::vector<AST::ExprNodeP> make_refpat_a(
        const Span& sp,
        ::std::vector< ::std::pair<RcString, AST::Pattern> >&   pats_a,
        const ::std::vector<AST::StructItem>& fields,
        ::std::function<AST::ExprNodeP(size_t, AST::ExprNodeP)> cb
    )
{
    ::std::vector<AST::ExprNodeP>   nodes;
    size_t idx = 0;
    for( const auto& fld : fields )
    {
        auto name_a = RcString::new_interned(FMT("a" << fld.m_name));
        pats_a.push_back( ::std::make_pair(fld.m_name, ::AST::Pattern(::AST::Pattern::TagBind(), sp, name_a, ::AST::PatternBinding::Type::REF)) );
        nodes.push_back( cb(idx, NEWNODE(NamedValue, AST::Path(name_a))) );
        idx ++;
    }
    return nodes;
}
static ::std::vector<AST::ExprNodeP> make_refpat_ab(
        const Span& sp,
        ::std::vector<AST::Pattern>&    pats_a,
        ::std::vector<AST::Pattern>&    pats_b,
        const ::std::vector<TypeRef>&   sub_types,
        ::std::function<AST::ExprNodeP(size_t, AST::ExprNodeP, AST::ExprNodeP)> cb
    )
{
    ::std::vector<AST::ExprNodeP>   nodes;
    for( size_t idx = 0; idx < sub_types.size(); idx ++ )
    {
        auto name_a = RcString::new_interned(FMT("a" << idx));
        auto name_b = RcString::new_interned(FMT("b" << idx));
        pats_a.push_back( ::AST::Pattern(::AST::Pattern::TagBind(), sp, name_a, ::AST::PatternBinding::Type::REF) );
        pats_b.push_back( ::AST::Pattern(::AST::Pattern::TagBind(), sp, name_b, ::AST::PatternBinding::Type::REF) );
        nodes.push_back( cb(idx, NEWNODE(NamedValue, AST::Path(name_a)), NEWNODE(NamedValue, AST::Path(name_b))) );
    }
    return nodes;
}
static ::std::vector<AST::ExprNodeP> make_refpat_ab(
        const Span& sp,
        ::std::vector< ::std::pair<RcString, AST::Pattern> >&   pats_a,
        ::std::vector< ::std::pair<RcString, AST::Pattern> >&   pats_b,
        const ::std::vector<AST::StructItem>& fields,
        ::std::function<AST::ExprNodeP(size_t, AST::ExprNodeP, AST::ExprNodeP)> cb
    )
{
    ::std::vector<AST::ExprNodeP>   nodes;
    size_t idx = 0;
    for( const auto& fld : fields )
    {
        auto name_a = RcString::new_interned(FMT("a" << fld.m_name));
        auto name_b = RcString::new_interned(FMT("b" << fld.m_name));
        pats_a.push_back( ::std::make_pair(fld.m_name, ::AST::Pattern(::AST::Pattern::TagBind(), sp, name_a, ::AST::PatternBinding::Type::REF)) );
        pats_b.push_back( ::std::make_pair(fld.m_name, ::AST::Pattern(::AST::Pattern::TagBind(), sp, name_b, ::AST::PatternBinding::Type::REF)) );
        nodes.push_back( cb(idx, NEWNODE(NamedValue, AST::Path(name_a)), NEWNODE(NamedValue, AST::Path(name_b))) );
        idx ++;
    }
    return nodes;
}


struct DeriveOpts
{
    RcString core_name;
    const ::std::vector<::AST::Attribute>&  derive_items;
};

/// Interface for derive handlers
struct Deriver
{
    virtual const char* trait_name() const = 0;
    virtual AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const = 0;
    virtual AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const = 0;
    virtual AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Union& unn) const {
        ERROR(sp, E0000, "Cannot derive(" << trait_name() << ") on union");
    }


    AST::GenericParams get_params_with_bounds(const Span& sp, const AST::GenericParams& p, const AST::Path& trait_path, ::std::vector<TypeRef> additional_bounded_types) const
    {
        AST::GenericParams  params = p.clone();

        // TODO: Get bounds based on generic (or similar) types used within the type.
        // - How would this code (that runs before resolve) know what's a generic and what's a local type?
        // - Searches within the type for a Path that starts with that param.

        unsigned int i = 0;
        for(const auto& arg : params.ty_params())
        {
            params.add_bound( ::AST::GenericBound::make_IsTrait({
                {}, TypeRef(sp, arg.name(), i), {}, trait_path
                }) );
            i ++;
        }

        // For each field type
        // - Locate used generic parameters in the type (and sub-types that directly use said parameter)
        for(auto& ty : additional_bounded_types)
        {
            params.add_bound( ::AST::GenericBound::make_IsTrait({
                {}, mv$(ty), {}, trait_path
                }) );
        }

        return params;
    }


    ::std::vector<TypeRef> get_field_bounds(const AST::Struct& str) const
    {
        ::std::vector<TypeRef>  ret;
        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            ),
        (Struct,
            for( const auto& fld : e.ents )
            {
                add_field_bound_from_ty(str.params(), ret, fld.m_type);
            }
            ),
        (Tuple,
            for(const auto& ent : e.ents)
            {
                add_field_bound_from_ty(str.params(), ret, ent.m_type);
            }
            )
        )
        return ret;
    }
    ::std::vector<TypeRef> get_field_bounds(const AST::Enum& enm) const
    {
        ::std::vector<TypeRef>  ret;

        for(const auto& v : enm.variants())
        {
            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                ),
            (Tuple,
                for(const auto& ty : e.m_sub_types)
                {
                    add_field_bound_from_ty(enm.params(), ret, ty);
                }
                ),
            (Struct,
                for( const auto& fld : e.m_fields )
                {
                    add_field_bound_from_ty(enm.params(), ret, fld.m_type);
                }
                )
            )
        }

        return ret;
    }
    ::std::vector<TypeRef> get_field_bounds(const AST::Union& unn) const
    {
        ::std::vector<TypeRef>  ret;
        for( const auto& fld : unn.m_variants )
        {
            add_field_bound_from_ty(unn.params(), ret, fld.m_type);
        }
        return ret;
    }

    void add_field_bound_from_ty(const AST::GenericParams& params, ::std::vector<TypeRef>& out_list, const TypeRef& ty) const
    {
        struct H {
            static void visit_nodes(const Deriver& self, const AST::GenericParams& params, ::std::vector<TypeRef>& out_list, const ::std::vector<AST::PathNode>& nodes) {
                for(const auto& node : nodes) {
                    for(const auto& ty : node.args().m_types) {
                        self.add_field_bound_from_ty(params, out_list, ty);
                    }
                    for(const auto& aty : node.args().m_assoc) {
                        self.add_field_bound_from_ty(params, out_list, aty.second);
                    }
                }
            }
        };
        // TODO: Locate type that is directly related to the type param.
        TU_MATCH(TypeData, (ty.m_data), (e),
        (None,
            // Wat?
            ),
        (Any,
            // Nope.
            ),
        (Unit,
            ),
        (Bang,
            ),
        (Macro,
            // not allowed
            ),
        (Primitive,
            ),
        (Function,
            // TODO? Well... function types don't tend to depend on the trait?
            ),
        (Tuple,
            for(const auto& sty : e.inner_types) {
                add_field_bound_from_ty(params, out_list, sty);
            }
            ),
        (Borrow,
            add_field_bound_from_ty(params, out_list, *e.inner);
            ),
        (Pointer,
            add_field_bound_from_ty(params, out_list, *e.inner);
            ),
        (Array,
            add_field_bound_from_ty(params, out_list, *e.inner);
            ),
        (Generic,
            // Although this is what we're looking for, it's already handled.
            ),
        (Path,
            TU_MATCH(AST::Path::Class, (e.path.m_class), (pe),
            (Invalid,
                // wut.
                ),
            (Local,
                ),
            (Relative,
                if( pe.nodes.size() > 1 )
                {
                    // Check if the first node of a relative is a generic param.
                    for(const auto& typ : params.ty_params())
                    {
                        if( pe.nodes.front().name() == typ.name() )
                        {
                            add_field_bound(out_list, ty);
                            break ;
                        }
                    }
                }
                H::visit_nodes(*this, params, out_list, pe.nodes);
                ),
            (Self,
                ),
            (Super,
                ),
            (Absolute,
                ),
            (UFCS,
                )
            )
            ),
        (TraitObject,
            // TODO: Should this be recursed?
            ),
        (ErasedType,
            // TODO: Should this be recursed?
            )
        )
    }
    void add_field_bound(::std::vector<TypeRef>& out_list, const TypeRef& type) const
    {
        for( const auto& ty : out_list )
        {
            if( ty == type ) {
                return ;
            }
        }

        out_list.push_back(type.clone());
    }
};

/// 'Debug' derive handler
class Deriver_Debug:
    public Deriver
{
    //static AST::ExprNodeP _print_l(::std::string val)
    //{
    //    return NEWNODE(CallMethod,
    //            NEWNODE(NamedValue, AST::Path("f")),
    //            AST::PathNode("write_str",{}),
    //            { NEWNODE(String, mv$(val)) }
    //            );
    //}
    //static AST::ExprNodeP _try(AST::ExprNodeP expr)
    //{
    //    throw CompileError::Todo("derive(Debug) - _try");
    //}

    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    debug_trait = get_path(core_name, "fmt", "Debug");

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(sp, get_path(core_name, "fmt", "Result")),
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"),
                    TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) ),
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "f"),
                    TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), true, TypeRef(sp, get_path(core_name, "fmt", "Formatter")) ) )
                )
            );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, debug_trait, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, debug_trait), type.clone() ) );
        rv.add_function(false, false, "fmt", mv$(fcn));
        return mv$(rv);
    }

public:
    const char* trait_name() const override { return "Debug"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        ::std::string name = type.path().nodes().back().name().c_str();

        // Generate code for Debug
        AST::ExprNodeP  node;
        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            node = NEWNODE(NamedValue, AST::Path("f"));
            node = NEWNODE(CallMethod,
                mv$(node), AST::PathNode("write_str",{}),
                vec$( NEWNODE(String, name) )
                );
            ),
        (Struct,
            node = NEWNODE(NamedValue, AST::Path("f"));
            node = NEWNODE(CallMethod,
                mv$(node), AST::PathNode("debug_struct",{}),
                vec$( NEWNODE(String, name) )
                );
            // TODO: use a block instead of chaining
            for( const auto& fld : e.ents )
            {
                node = NEWNODE(CallMethod,
                    mv$(node), AST::PathNode("field",{}),
                    vec$(
                        NEWNODE(String, fld.m_name.c_str()),
                        NEWNODE(UniOp, AST::ExprNode_UniOp::REF, NEWNODE(UniOp, AST::ExprNode_UniOp::REF,
                            NEWNODE(Field,
                                NEWNODE(NamedValue, AST::Path("self")),
                                fld.m_name
                                )
                            ))
                    )
                    );
            }
            node = NEWNODE(CallMethod, mv$(node), AST::PathNode("finish",{}), {});
            ),
        (Tuple,
            node = NEWNODE(NamedValue, AST::Path("f"));
            node = NEWNODE(CallMethod,
                mv$(node), AST::PathNode("debug_tuple",{}),
                vec$( NEWNODE(String, name) )
                );
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                node = NEWNODE(CallMethod,
                    mv$(node), AST::PathNode("field",{}),
                    vec$(
                        NEWNODE(UniOp, AST::ExprNode_UniOp::REF, NEWNODE(UniOp, AST::ExprNode_UniOp::REF,
                            NEWNODE(Field,
                                NEWNODE(NamedValue, AST::Path("self")),
                                RcString::new_interned(FMT(idx))
                                )
                            ))
                        )
                    );
            }
            node = NEWNODE(CallMethod, mv$(node), AST::PathNode("finish",{}), {});
            )
        )

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), mv$(node));
    }
    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back() = base_path.nodes().back().name();

        ::std::vector< AST::ExprNode_Match_Arm> arms;
        for(const auto& v : enm.variants())
        {
            AST::ExprNodeP  code;
            AST::Pattern    pat_a;

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = NEWNODE(CallMethod,
                    NEWNODE(NamedValue, AST::Path("f")),
                    AST::PathNode("write_str",{}),
                    vec$( NEWNODE(String, v.m_name.c_str()) )
                    );
                pat_a = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                ),
            (Tuple,
                ::std::vector<AST::Pattern>    pats_a;

                auto s_ent = NEWNODE(NamedValue, AST::Path("s"));
                auto nodes = make_refpat_a(sp, pats_a, e.m_sub_types, [&](size_t idx, auto a){
                    return NEWNODE(CallMethod, s_ent->clone(), AST::PathNode("field", {}), vec$( mv$(a) ));
                    });
                nodes.insert(nodes.begin(), NEWNODE(LetBinding, AST::Pattern(AST::Pattern::TagBind(), sp, "s"), TypeRef(sp),
                    NEWNODE(CallMethod, NEWNODE(NamedValue, AST::Path("f")), AST::PathNode("debug_tuple",{}),
                        vec$( NEWNODE(String, v.m_name.c_str()) )
                        )
                        ));
                nodes.push_back( NEWNODE(CallMethod, mv$(s_ent), AST::PathNode("finish",{}), {}) );
                code = NEWNODE(Block, mv$(nodes));
                pat_a = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_a));
                ),
            (Struct,
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_a;

                auto s_ent = NEWNODE(NamedValue, AST::Path("s"));
                auto nodes = make_refpat_a(sp, pats_a, e.m_fields, [&](size_t idx, auto a){
                    return NEWNODE(CallMethod, s_ent->clone(), AST::PathNode("field", {}),
                            vec$(
                                NEWNODE(String, e.m_fields[idx].m_name.c_str()),
                                mv$(a)
                                )
                            );
                    });
                nodes.insert(nodes.begin(), NEWNODE(LetBinding, AST::Pattern(AST::Pattern::TagBind(), sp, "s"), TypeRef(sp),
                    NEWNODE(CallMethod, NEWNODE(NamedValue, AST::Path("f")), AST::PathNode("debug_struct",{}),
                        vec$( NEWNODE(String, v.m_name.c_str()) )
                        )
                        ));
                nodes.push_back( NEWNODE(CallMethod, mv$(s_ent), AST::PathNode("finish",{}), {}) );

                code = NEWNODE(Block, mv$(nodes));
                pat_a = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_a), true);
                )
            )

            ::std::vector< AST::Pattern>    pats;
            pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_a)) );

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }
        AST::ExprNodeP  node = NEWNODE(Match,
            NEWNODE(NamedValue, AST::Path("self")),
            mv$(arms)
            );

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), mv$(node));
    }
} g_derive_debug;

class Deriver_PartialEq:
    public Deriver
{
    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    trait_path = get_path(core_name, "cmp", "PartialEq");

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(sp, CORETYPE_BOOL),
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) ),
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "v"   ), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) )
                )
            );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "eq", mv$(fcn));
        return mv$(rv);
    }
    AST::ExprNodeP compare_and_ret(Span sp, const RcString& core_name, AST::ExprNodeP v1, AST::ExprNodeP v2) const
    {
        return NEWNODE(If,
            NEWNODE(BinOp, AST::ExprNode_BinOp::CMPNEQU, mv$(v1), mv$(v2)),
            NEWNODE(Flow, AST::ExprNode_Flow::RETURN, "", NEWNODE(Bool, false)),
            nullptr
            );
    }
public:
    const char* trait_name() const override { return "PartialEq"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        ::std::vector<AST::ExprNodeP>   nodes;

        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            ),
        (Struct,
            for( const auto& fld : e.ents )
            {
                nodes.push_back(this->compare_and_ret( sp, opts.core_name,
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), fld.m_name),
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("v"   )), fld.m_name)
                    ));
            }
            ),
        (Tuple,
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                auto fld_name = RcString::new_interned(FMT(idx));
                nodes.push_back(this->compare_and_ret( sp, opts.core_name,
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), fld_name),
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("v"   )), fld_name)
                    ));
            }
            )
        )
        nodes.push_back( NEWNODE(Bool, true) );

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), NEWNODE(Block, mv$(nodes)));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back().args() = ::AST::PathParams();
        ::std::vector<AST::ExprNode_Match_Arm>   arms;

        for(const auto& v : enm.variants())
        {
            AST::ExprNodeP  code;
            AST::Pattern    pat_a;
            AST::Pattern    pat_b;

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = NEWNODE(Bool, true);
                pat_a = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                pat_b = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                ),
            (Tuple,
                ::std::vector<AST::Pattern>    pats_a;
                ::std::vector<AST::Pattern>    pats_b;

                auto nodes = make_refpat_ab(sp, pats_a, pats_b, e.m_sub_types, [&](auto idx, auto a, auto b){
                        return this->compare_and_ret(sp, opts.core_name, mv$(a), mv$(b));
                        });
                nodes.push_back( NEWNODE(Bool, true) );

                pat_a = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_a));
                pat_b = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_b));
                code = NEWNODE(Block, mv$(nodes));
                ),
            (Struct,
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_a;
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_b;

                auto nodes = make_refpat_ab(sp, pats_a, pats_b, e.m_fields, [&](const auto& name, auto a, auto b){
                        return this->compare_and_ret(sp, opts.core_name, mv$(a), mv$(b));
                        });
                nodes.push_back( NEWNODE(Bool, true) );

                pat_a = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_a), true);
                pat_b = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_b), true);
                code = NEWNODE(Block, mv$(nodes));
                )
            )

            ::std::vector< AST::Pattern>    pats;
            {
                ::std::vector< AST::Pattern>    tuple_pats;
                tuple_pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_a)) );
                tuple_pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_b)) );
                pats.push_back( AST::Pattern(AST::Pattern::TagTuple(), sp, mv$(tuple_pats)) );
            }

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }

        // Default arm
        {
            arms.push_back(AST::ExprNode_Match_Arm(
                ::make_vec1( AST::Pattern() ),
                nullptr,
                NEWNODE(Bool, false)
                ));
        }

        ::std::vector<AST::ExprNodeP>   vals;
        vals.push_back( NEWNODE(NamedValue, AST::Path("self")) );
        vals.push_back( NEWNODE(NamedValue, AST::Path("v")) );
        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), NEWNODE(Match,
            NEWNODE(Tuple, mv$(vals)),
            mv$(arms)
            ));
    }
} g_derive_partialeq;

class Deriver_PartialOrd:
    public Deriver
{
    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path trait_path = get_path(core_name, "cmp", "PartialOrd");
        const AST::Path path_ordering = get_path(core_name, "cmp", "Ordering");

        AST::Path path_option_ordering = get_path(core_name, "option", "Option");
        path_option_ordering.nodes().back().args().m_types.push_back( TypeRef(sp, path_ordering) );

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(sp, path_option_ordering),
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) ),
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "v"   ), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) )
                )
            );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "partial_cmp", mv$(fcn));
        return mv$(rv);
    }

    AST::ExprNodeP make_compare_and_ret(Span sp, const RcString& core_name, AST::ExprNodeP v1, AST::ExprNodeP v2) const
    {
        return NEWNODE(Match,
            NEWNODE(CallPath, get_path(core_name, "cmp", "PartialOrd", "partial_cmp"),
                ::make_vec2(
                    NEWNODE(UniOp, AST::ExprNode_UniOp::REF, mv$(v1)),
                    NEWNODE(UniOp, AST::ExprNode_UniOp::REF, mv$(v2))
                    )
                ),
            ::make_vec3(
                ::AST::ExprNode_Match_Arm(
                    ::make_vec1( AST::Pattern(AST::Pattern::TagValue(), sp, get_path(core_name, "option", "Option", "None")) ),
                    nullptr,
                    NEWNODE(Flow, AST::ExprNode_Flow::RETURN, "", NEWNODE(NamedValue, get_path(core_name, "option", "Option", "None")))
                    ),
                ::AST::ExprNode_Match_Arm(
                    ::make_vec1( AST::Pattern(AST::Pattern::TagNamedTuple(), sp, get_path(core_name, "option", "Option", "Some"),
                        ::make_vec1(  AST::Pattern(AST::Pattern::TagValue(), sp, get_path(core_name, "cmp", "Ordering", "Equal")) )
                        ) ),
                    nullptr,
                    NEWNODE(Tuple, ::std::vector<AST::ExprNodeP>())
                    ),
                ::AST::ExprNode_Match_Arm(
                    ::make_vec1( AST::Pattern(AST::Pattern::TagBind(), sp, "res") ),
                    nullptr,
                    NEWNODE(Flow, AST::ExprNode_Flow::RETURN, "", NEWNODE(NamedValue, AST::Path("res")))
                    )
                )
            );
    }
    AST::ExprNodeP make_ret_equal(const RcString& core_name) const
    {
        return NEWNODE(CallPath, get_path(core_name, "option", "Option", "Some"),
            ::make_vec1( NEWNODE(NamedValue, get_path(core_name, "cmp", "Ordering", "Equal")) )
            );
    }
public:
    const char* trait_name() const override { return "PartialOrd"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        ::std::vector<AST::ExprNodeP>   nodes;

        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            ),
        (Struct,
            for( const auto& fld : e.ents )
            {
                nodes.push_back(this->make_compare_and_ret( sp, opts.core_name,
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), fld.m_name),
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("v"   )), fld.m_name)
                    ));
            }
            ),
        (Tuple,
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                auto fld_name = RcString::new_interned(FMT(idx));
                nodes.push_back(this->make_compare_and_ret( sp, opts.core_name,
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), fld_name),
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("v"   )), fld_name)
                    ));
            }
            )
        )
        nodes.push_back( this->make_ret_equal(opts.core_name) );

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), NEWNODE(Block, mv$(nodes)));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back().args() = ::AST::PathParams();
        ::std::vector<AST::ExprNode_Match_Arm>   arms;

        for(const auto& v : enm.variants())
        {
            AST::ExprNodeP  code;
            AST::Pattern    pat_a;
            AST::Pattern    pat_b;

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = this->make_ret_equal(opts.core_name);
                pat_a = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                pat_b = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                ),
            (Tuple,
                ::std::vector<AST::Pattern>    pats_a;
                ::std::vector<AST::Pattern>    pats_b;

                auto nodes = make_refpat_ab(sp, pats_a, pats_b, e.m_sub_types, [&](size_t , auto a, auto b){
                        return this->make_compare_and_ret(sp, opts.core_name, NEWNODE(Deref, mv$(a)), NEWNODE(Deref, mv$(b)));
                        });
                nodes.push_back( this->make_ret_equal(opts.core_name) );

                pat_a = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_a));
                pat_b = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_b));
                code = NEWNODE(Block, mv$(nodes));
                ),
            (Struct,
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_a;
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_b;

                auto nodes = make_refpat_ab(sp, pats_a, pats_b, e.m_fields, [&](size_t /*idx*/, auto a, auto b){
                        return this->make_compare_and_ret(sp, opts.core_name, NEWNODE(Deref, mv$(a)), NEWNODE(Deref, mv$(b)));
                        });
                nodes.push_back( this->make_ret_equal(opts.core_name) );

                pat_a = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_a), true);
                pat_b = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_b), true);
                code = NEWNODE(Block, mv$(nodes));
                )
            )

            ::std::vector< AST::Pattern>    pats;
            {
                ::std::vector< AST::Pattern>    tuple_pats;
                tuple_pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_a)) );
                tuple_pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_b)) );
                pats.push_back( AST::Pattern(AST::Pattern::TagTuple(), sp, mv$(tuple_pats)) );
            }

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }

        // TODO: Compare the discriminants using the `discriminant_value` intrinsic
        // - Requires a way of emitting said intrinsic into the AST
        // - LAZY WAY - hard-code ::"core"::intrinsics::discriminant_value
        {
            auto code = NEWNODE(CallPath, get_path(opts.core_name, "cmp", "PartialOrd", "partial_cmp"),
                ::make_vec2(
                    NEWNODE(UniOp, AST::ExprNode_UniOp::REF, NEWNODE(CallPath, get_path(opts.core_name, "intrinsics", "discriminant_value"), make_vec1( NEWNODE(NamedValue, AST::Path("self")) )) ),
                    NEWNODE(UniOp, AST::ExprNode_UniOp::REF, NEWNODE(CallPath, get_path(opts.core_name, "intrinsics", "discriminant_value"), make_vec1( NEWNODE(NamedValue, AST::Path("v")) )) )
                    )
                );
            ::std::vector< AST::Pattern>    pats = make_vec1( AST::Pattern() );

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }

        ::std::vector<AST::ExprNodeP>   vals;
        vals.push_back( NEWNODE(NamedValue, AST::Path("self")) );
        vals.push_back( NEWNODE(NamedValue, AST::Path("v")) );
        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), NEWNODE(Match,
            NEWNODE(Tuple, mv$(vals)),
            mv$(arms)
            ));
    }
} g_derive_partialord;

class Deriver_Eq:
    public Deriver
{
    AST::Path get_trait_path(const RcString& core_name) const {
        return get_path(core_name, "cmp", "Eq");
    }

    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    trait_path = this->get_trait_path(core_name);

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(TypeRef::TagUnit(), sp),
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) )
                )
            );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "assert_receiver_is_total_eq", mv$(fcn));
        return mv$(rv);
    }
    AST::ExprNodeP assert_is_eq(const AST::Path& method_path, AST::ExprNodeP val) const {
        return NEWNODE(CallPath,
            AST::Path(method_path),
            vec$( NEWNODE(UniOp, AST::ExprNode_UniOp::REF, mv$(val) ) )
            );
    }
    AST::ExprNodeP field(const ::std::string& name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), RcString::new_interned(name));
    }
    AST::ExprNodeP field(const RcString& name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), name);
    }

public:
    const char* trait_name() const override { return "Eq"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        const AST::Path    assert_method_path = this->get_trait_path(opts.core_name) + "assert_receiver_is_total_eq";
        ::std::vector<AST::ExprNodeP>   nodes;

        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            ),
        (Struct,
            for( const auto& fld : e.ents )
            {
                nodes.push_back( this->assert_is_eq(assert_method_path, this->field(fld.m_name)) );
            }
            ),
        (Tuple,
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                nodes.push_back( this->assert_is_eq(assert_method_path, this->field(FMT(idx))) );
            }
            )
        )

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), NEWNODE(Block, mv$(nodes)));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        const AST::Path    assert_method_path = this->get_trait_path(opts.core_name) + "assert_receiver_is_total_eq";

        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back().args() = ::AST::PathParams();
        ::std::vector<AST::ExprNode_Match_Arm>   arms;

        for(const auto& v : enm.variants())
        {
            AST::ExprNodeP  code;
            AST::Pattern    pat_a;

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = NEWNODE(Block);
                pat_a = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                ),
            (Tuple,
                ::std::vector<AST::Pattern>    pats_a;
                auto nodes = make_refpat_a(sp, pats_a, e.m_sub_types, [&](size_t idx, auto a){
                    return this->assert_is_eq(assert_method_path, mv$(a));
                    });

                pat_a = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_a));
                code = NEWNODE(Block, mv$(nodes));
                ),
            (Struct,
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_a;
                auto nodes = make_refpat_a(sp, pats_a, e.m_fields, [&](size_t idx, auto a){
                    return this->assert_is_eq(assert_method_path, mv$(a));
                    });

                pat_a = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_a), true);
                code = NEWNODE(Block, mv$(nodes));
                )
            )

            ::std::vector< AST::Pattern>    pats;
            pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_a)) );

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), NEWNODE(Match,
            NEWNODE(NamedValue, AST::Path("self")),
            mv$(arms)
            ));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Union& unn) const override
    {
        // Eq is just a marker, so it's valid to derive for union
        const AST::Path    assert_method_path = this->get_trait_path(opts.core_name) + "assert_receiver_is_total_eq";
        ::std::vector<AST::ExprNodeP>   nodes;

        for( const auto& fld : unn.m_variants )
        {
            nodes.push_back( this->assert_is_eq(assert_method_path, this->field(fld.m_name)) );
        }

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(unn), NEWNODE(Block, mv$(nodes)));
    }
} g_derive_eq;

class Deriver_Ord:
    public Deriver
{
    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path trait_path = get_path(core_name, "cmp", "Ord");
        const AST::Path path_ordering = get_path(core_name, "cmp", "Ordering");

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(sp, path_ordering),
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) ),
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "v"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) )
                )
            );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "cmp", mv$(fcn));
        return mv$(rv);
    }

    AST::ExprNodeP make_compare_and_ret(Span sp, const RcString& core_name, AST::ExprNodeP v1, AST::ExprNodeP v2) const
    {
        return NEWNODE(Match,
            NEWNODE(CallPath, get_path(core_name, "cmp", "Ord", "cmp"),
                // TODO: Optional Ref?
                ::make_vec2(
                    NEWNODE(UniOp, AST::ExprNode_UniOp::REF, mv$(v1)),
                    NEWNODE(UniOp, AST::ExprNode_UniOp::REF, mv$(v2))
                    )
                ),
            ::make_vec2(
                ::AST::ExprNode_Match_Arm(
                    ::make_vec1( AST::Pattern(AST::Pattern::TagValue(), sp, get_path(core_name, "cmp", "Ordering", "Equal")) ),
                    nullptr,
                    NEWNODE(Tuple, ::std::vector<AST::ExprNodeP>())
                    ),
                ::AST::ExprNode_Match_Arm(
                    ::make_vec1( AST::Pattern(AST::Pattern::TagBind(), sp, "res") ),
                    nullptr,
                    NEWNODE(Flow, AST::ExprNode_Flow::RETURN, "", NEWNODE(NamedValue, AST::Path("res")))
                    )
                )
            );
    }
    AST::ExprNodeP make_ret_equal(const RcString& core_name) const
    {
        return NEWNODE(NamedValue, get_path(core_name, "cmp", "Ordering", "Equal"));
    }
public:
    const char* trait_name() const override { return "Ord"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        ::std::vector<AST::ExprNodeP>   nodes;

        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            ),
        (Struct,
            for( const auto& fld : e.ents )
            {
                nodes.push_back(this->make_compare_and_ret( sp, opts.core_name,
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), fld.m_name),
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("v"   )), fld.m_name)
                    ));
            }
            ),
        (Tuple,
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                auto fld_name = RcString::new_interned(FMT(idx));
                nodes.push_back(this->make_compare_and_ret( sp, opts.core_name,
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), fld_name),
                    NEWNODE(Field, NEWNODE(NamedValue, AST::Path("v"   )), fld_name)
                    ));
            }
            )
        )
        nodes.push_back( this->make_ret_equal(opts.core_name) );

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), NEWNODE(Block, mv$(nodes)));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back().args() = ::AST::PathParams();
        ::std::vector<AST::ExprNode_Match_Arm>   arms;

        for(const auto& v : enm.variants())
        {
            AST::ExprNodeP  code;
            AST::Pattern    pat_a;
            AST::Pattern    pat_b;

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = this->make_ret_equal(opts.core_name);
                pat_a = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                pat_b = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                ),
            (Tuple,
                ::std::vector<AST::Pattern>    pats_a;
                ::std::vector<AST::Pattern>    pats_b;

                auto nodes = make_refpat_ab(sp, pats_a, pats_b, e.m_sub_types, [&](size_t, auto a, auto b){
                        return this->make_compare_and_ret(sp, opts.core_name, mv$(a), mv$(b));
                        });
                nodes.push_back( this->make_ret_equal(opts.core_name) );

                pat_a = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_a));
                pat_b = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_b));
                code = NEWNODE(Block, mv$(nodes));
                ),
            (Struct,
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_a;
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_b;

                auto nodes = make_refpat_ab(sp, pats_a, pats_b, e.m_fields, [&](const auto& /*name*/, auto a, auto b) {
                        return this->make_compare_and_ret(sp, opts.core_name, mv$(a), mv$(b));
                        });
                nodes.push_back( this->make_ret_equal(opts.core_name) );

                pat_a = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_a), true);
                pat_b = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_b), true);
                code = NEWNODE(Block, mv$(nodes));
                )
            )

            ::std::vector< AST::Pattern>    pats;
            {
                ::std::vector< AST::Pattern>    tuple_pats;
                tuple_pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_a)) );
                tuple_pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_b)) );
                pats.push_back( AST::Pattern(AST::Pattern::TagTuple(), sp, mv$(tuple_pats)) );
            }

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }


        {
            auto code = NEWNODE(CallPath, get_path(opts.core_name, "cmp", "Ord", "cmp"),
                ::make_vec2(
                    NEWNODE(UniOp, AST::ExprNode_UniOp::REF, NEWNODE(CallPath, get_path(opts.core_name, "intrinsics", "discriminant_value"), make_vec1( NEWNODE(NamedValue, AST::Path("self")) )) ),
                    NEWNODE(UniOp, AST::ExprNode_UniOp::REF, NEWNODE(CallPath, get_path(opts.core_name, "intrinsics", "discriminant_value"), make_vec1( NEWNODE(NamedValue, AST::Path("v")) )) )
                    )
                );
            ::std::vector< AST::Pattern>    pats = make_vec1( AST::Pattern() );

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }

        ::std::vector<AST::ExprNodeP>   vals;
        vals.push_back( NEWNODE(NamedValue, AST::Path("self")) );
        vals.push_back( NEWNODE(NamedValue, AST::Path("v")) );
        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), NEWNODE(Match,
            NEWNODE(Tuple, mv$(vals)),
            mv$(arms)
            ));
    }
} g_derive_ord;

class Deriver_Clone:
    public Deriver
{
    AST::Path get_trait_path(const RcString& core_name) const {
        return AST::Path(core_name, { AST::PathNode("clone", {}), AST::PathNode("Clone", {}) });
    }
    AST::Path get_method_path(const RcString& core_name) const {
        return get_trait_path(core_name) + "clone";
    }

    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    trait_path = this->get_trait_path(core_name);

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(sp, "Self", 0xFFFF),
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) )
                )
            );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "clone", mv$(fcn));
        return mv$(rv);
    }
    AST::ExprNodeP clone_val_ref(const RcString& core_name, AST::ExprNodeP val) const {
        // TODO: Hack for zero-sized arrays? (Not a 1.19 feature)
        return NEWNODE(CallPath,
            this->get_method_path(core_name),
            vec$( NEWNODE(UniOp, AST::ExprNode_UniOp::REF, mv$(val) ) )
            );
    }
    AST::ExprNodeP clone_val_direct(const RcString& core_name, AST::ExprNodeP val) const {
        return NEWNODE(CallPath,
            this->get_method_path(core_name),
            vec$( mv$(val) )
            );
    }
    AST::ExprNodeP field(const RcString& name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), name);
    }
    AST::ExprNodeP field(const ::std::string& name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), RcString::new_interned(name));
    }

public:
    const char* trait_name() const override { return "Clone"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        const AST::Path& ty_path = type.m_data.as_Path().path;
        ::std::vector<AST::ExprNodeP>   nodes;

        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            nodes.push_back( NEWNODE(NamedValue, AST::Path(ty_path)) );
            ),
        (Struct,
            ::AST::ExprNode_StructLiteral::t_values vals;
            for( const auto& fld : e.ents )
            {
                vals.push_back({ {}, fld.m_name, this->clone_val_ref(opts.core_name, this->field(fld.m_name)) });
            }
            nodes.push_back( NEWNODE(StructLiteral, ty_path, nullptr, mv$(vals)) );
            ),
        (Tuple,
            ::std::vector<AST::ExprNodeP>   vals;
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                vals.push_back( this->clone_val_ref(opts.core_name, this->field(FMT(idx))) );
            }
            nodes.push_back( NEWNODE(CallPath, AST::Path(ty_path), mv$(vals)) );
            )
        )

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), NEWNODE(Block, mv$(nodes)));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back().args() = ::AST::PathParams();
        ::std::vector<AST::ExprNode_Match_Arm>   arms;

        for(const auto& v : enm.variants())
        {
            AST::ExprNodeP  code;
            AST::Pattern    pat_a;

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = NEWNODE(NamedValue, base_path + v.m_name);
                pat_a = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                ),
            (Tuple,
                ::std::vector<AST::Pattern>    pats_a;
                auto nodes = make_refpat_a(sp, pats_a, e.m_sub_types, [&](size_t , auto a) {
                    return this->clone_val_direct(opts.core_name, mv$(a));
                    });

                pat_a = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_a));
                code = NEWNODE(CallPath, base_path + v.m_name, mv$(nodes));
                ),
            (Struct,
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_a;
                ::AST::ExprNode_StructLiteral::t_values vals;

                for( const auto& fld : e.m_fields )
                {
                    auto name_a = RcString::new_interned(FMT("a" << fld.m_name));
                    pats_a.push_back( ::std::make_pair(fld.m_name, ::AST::Pattern(::AST::Pattern::TagBind(), sp, name_a, ::AST::PatternBinding::Type::REF)) );
                    vals.push_back({ {}, fld.m_name, this->clone_val_direct(opts.core_name, NEWNODE(NamedValue, AST::Path(name_a))) });
                }

                pat_a = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_a), true);
                code = NEWNODE(StructLiteral, base_path + v.m_name, nullptr, mv$(vals));
                )
            )

            ::std::vector< AST::Pattern>    pats;
            pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_a)) );

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), NEWNODE(Match,
            NEWNODE(NamedValue, AST::Path("self")),
            mv$(arms)
            ));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Union& unn) const override
    {
        return make_copy_clone(sp, opts, p, type, this->get_field_bounds(unn));
    }
private:
    AST::Impl make_copy_clone(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> field_bounds) const
    {
        // Clone on a union can only be a bitwise copy.
        // - This requires a Copy impl. That's up to the user
        auto ret = this->make_ret(sp, opts.core_name, p, type, ::std::move(field_bounds), NEWNODE(Deref,
            NEWNODE(NamedValue, AST::Path("self"))
            ));

        // TODO: What if the type is only conditionally copy? (generic over something)
        // - Could abuse specialisation support...
        // TODO: Are these bounds needed?
        for(auto& b : ret.def().params().bounds())
        {
            auto& be = b.as_IsTrait();
            be.trait = get_path(opts.core_name, "marker", "Copy");
        }

        return ret;
    }
} g_derive_clone;

class Deriver_Copy:
    public Deriver
{
    AST::Path get_trait_path(const RcString& core_name) const {
        return get_path(core_name, "marker", "Copy");
    }

    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    trait_path = this->get_trait_path(core_name);

        AST::GenericParams params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        return mv$(rv);
    }

public:
    const char* trait_name() const override { return "Copy"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), nullptr);
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), nullptr);
    }
    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Union& unn) const override
    {
        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(unn), nullptr);
    }
} g_derive_copy;

class Deriver_Default:
    public Deriver
{
    AST::Path get_trait_path(const RcString& core_name) const {
        return get_path(core_name, "default", "Default");
    }
    AST::Path get_method_path(const RcString& core_name) const {
        return AST::Path(AST::Path::TagUfcs(), ::TypeRef(Span()), get_trait_path(core_name), { AST::PathNode("default", {}) } );
    }

    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    trait_path = this->get_trait_path(core_name);

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(sp, "Self", 0xFFFF),
            {}
            );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "default", mv$(fcn));
        return mv$(rv);
    }
    AST::ExprNodeP default_call(const RcString& core_name) const {
        return NEWNODE(CallPath,
            this->get_method_path(core_name),
            {}
            );
    }

public:
    const char* trait_name() const override { return "Default"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        const AST::Path& ty_path = type.m_data.as_Path().path;
        ::std::vector<AST::ExprNodeP>   nodes;

        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            nodes.push_back( NEWNODE(NamedValue, AST::Path(ty_path)) );
            ),
        (Struct,
            ::AST::ExprNode_StructLiteral::t_values vals;
            for( const auto& fld : e.ents )
            {
                vals.push_back({ {}, fld.m_name, this->default_call(opts.core_name) });
            }
            nodes.push_back( NEWNODE(StructLiteral, ty_path, nullptr, mv$(vals)) );
            ),
        (Tuple,
            ::std::vector<AST::ExprNodeP>   vals;
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                vals.push_back( this->default_call(opts.core_name) );
            }
            nodes.push_back( NEWNODE(CallPath, AST::Path(ty_path), mv$(vals)) );
            )
        )

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), NEWNODE(Block, mv$(nodes)));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        ERROR(sp, E0000, "Default cannot be derived for enums");
    }
} g_derive_default;

class Deriver_Hash:
    public Deriver
{
    AST::Path get_trait_path(const RcString& core_name) const {
        return get_path(core_name, "hash", "Hash");
    }
    AST::Path get_trait_path_Hasher(const RcString& core_name) const {
        return get_path(core_name, "hash", "Hasher");
    }
    AST::Path get_method_path(const RcString& core_name) const {
        return get_trait_path(core_name) + "hash";
    }

    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    trait_path = this->get_trait_path(core_name);

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(TypeRef::TagUnit(), sp),
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) ),
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "state"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), true, TypeRef(sp, "H", 0x100|0)) )
                )
            );
        fcn.params().add_ty_param( AST::TypeParam(sp, {}, "H") );
        fcn.params().add_bound( AST::GenericBound::make_IsTrait({
            {}, TypeRef(sp, "H", 0x100|0),
            {}, this->get_trait_path_Hasher(core_name)
            }) );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "hash", mv$(fcn));
        return mv$(rv);
    }
    AST::ExprNodeP hash_val_ref(const RcString& core_name, AST::ExprNodeP val) const {
        return this->hash_val_direct(core_name, NEWNODE(UniOp, AST::ExprNode_UniOp::REF, mv$(val)) );
    }
    AST::ExprNodeP hash_val_direct(const RcString& core_name, AST::ExprNodeP val) const {
        return NEWNODE(CallPath,
            this->get_method_path(core_name),
            vec$( mv$(val), NEWNODE(NamedValue, AST::Path("state")) )
            );
    }
    AST::ExprNodeP field(const RcString& name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), name);
    }
    AST::ExprNodeP field(const std::string& name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), RcString::new_interned(name));
    }

public:
    const char* trait_name() const override { return "Hash"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        ::std::vector<AST::ExprNodeP>   nodes;

        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            ),
        (Struct,
            for( const auto& fld : e.ents )
            {
                nodes.push_back( this->hash_val_ref(opts.core_name, this->field(fld.m_name)) );
            }
            ),
        (Tuple,
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                nodes.push_back( this->hash_val_ref(opts.core_name, this->field(FMT(idx))) );
            }
            )
        )

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), NEWNODE(Block, mv$(nodes)));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back().args() = ::AST::PathParams();
        ::std::vector<AST::ExprNode_Match_Arm>   arms;

        for(unsigned int var_idx = 0; var_idx < enm.variants().size(); var_idx ++)
        {
            const auto& v = enm.variants()[var_idx];
            AST::ExprNodeP  code;
            AST::Pattern    pat_a;

            auto var_idx_hash = this->hash_val_ref( opts.core_name, NEWNODE(Integer, var_idx, CORETYPE_UINT) );

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = mv$(var_idx_hash);
                pat_a = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                ),
            (Tuple,
                ::std::vector<AST::Pattern>    pats_a;
                auto nodes = make_refpat_a(sp, pats_a, e.m_sub_types, [&](size_t , auto a) {
                        return this->hash_val_direct(opts.core_name, mv$(a));
                        });
                nodes.insert(nodes.begin(), mv$(var_idx_hash));

                pat_a = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_a));
                code = NEWNODE(Block, mv$(nodes));
                ),
            (Struct,
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_a;
                auto nodes = make_refpat_a(sp, pats_a, e.m_fields, [&](size_t , auto a) {
                        return this->hash_val_direct(opts.core_name, mv$(a));
                        });
                nodes.insert(nodes.begin(), mv$(var_idx_hash));

                pat_a = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_a), true);
                code = NEWNODE(Block, mv$(nodes));
                )
            )

            ::std::vector< AST::Pattern>    pats;
            pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_a)) );

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), NEWNODE(Match,
            NEWNODE(NamedValue, AST::Path("self")),
            mv$(arms)
            ));
    }
} g_derive_hash;

class Deriver_RustcEncodable:
    public Deriver
{
    // NOTE: This emits paths like `::rustc_serialize::Encodable` - rustc and crates.io have subtly different crate names
    AST::Path get_trait_path() const {
        return get_path("", "rustc_serialize", "Encodable");
    }
    AST::Path get_trait_path_Encoder() const {
        return get_path("", "rustc_serialize", "Encoder");
    }
    AST::Path get_method_path() const {
        return get_trait_path() + "encode";
    }

    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    trait_path = this->get_trait_path();

        AST::Path result_path = get_path(core_name, "result", "Result");
        result_path.nodes()[1].args().m_types.push_back( TypeRef(TypeRef::TagUnit(), sp) );
        result_path.nodes()[1].args().m_types.push_back(TypeRef( sp, AST::Path(AST::Path::TagUfcs(), TypeRef(sp, "S", 0x100|0), this->get_trait_path_Encoder(), { AST::PathNode("Error",{}) }) ));

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(sp, mv$(result_path)),
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), false, TypeRef(sp, "Self", 0xFFFF)) ),
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "s"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), true, TypeRef(sp, "S", 0x100|0)) )
                )
            );
        fcn.params().add_ty_param( AST::TypeParam(sp, {}, "S") );
        fcn.params().add_bound( AST::GenericBound::make_IsTrait({
            {}, TypeRef(sp, "S", 0x100|0),
            {}, this->get_trait_path_Encoder()
            }) );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "encode", mv$(fcn));
        return mv$(rv);
    }
    AST::ExprNodeP enc_val_direct(AST::ExprNodeP val) const {
        return NEWNODE(CallPath, this->get_method_path(),  vec$( mv$(val), NEWNODE(NamedValue, AST::Path("s")) ));
    }
    AST::ExprNodeP enc_val_ref(AST::ExprNodeP val) const {
        return this->enc_val_direct(NEWNODE(UniOp, AST::ExprNode_UniOp::REF, mv$(val)) );
    }
    AST::ExprNodeP field(const RcString& name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), name);
    }
    AST::ExprNodeP field(::std::string name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path("self")), RcString::new_interned(name));
    }

    AST::ExprNodeP enc_closure(Span sp, AST::ExprNodeP code) const {
        return NEWNODE(Closure,
            vec$( ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "s"), ::TypeRef(sp) ) ), ::TypeRef(sp),
            mv$(code), false
            );
    }
    AST::ExprNodeP get_val_ok(const RcString& core_name) const {
        return NEWNODE(CallPath, get_path(core_name, "result", "Result", "Ok"), vec$( NEWNODE(Tuple, {})) );
    }

public:
    const char* trait_name() const override { return "RustcEncodable"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        ::std::string struct_name = type.m_data.as_Path().path.nodes().back().name().c_str();

        ::std::vector<AST::ExprNodeP>   nodes;
        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            ),
        (Struct,
            unsigned int idx = 0;
            for( const auto& fld : e.ents )
            {
                nodes.push_back( NEWNODE(CallPath,
                    this->get_trait_path_Encoder() + "emit_struct_field",
                    vec$(
                        NEWNODE(NamedValue, AST::Path("s")),
                        NEWNODE(String, fld.m_name.c_str()),
                        NEWNODE(Integer, idx, CORETYPE_UINT),
                        this->enc_closure( sp, this->enc_val_ref(this->field(fld.m_name)) )
                        )
                    ) );
                idx ++;
            }
            ),
        (Tuple,
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                nodes.push_back( NEWNODE(CallPath,
                    this->get_trait_path_Encoder() + "emit_tuple_struct_arg",
                    vec$( NEWNODE(NamedValue, AST::Path("s")), NEWNODE(Integer, idx, CORETYPE_UINT), this->enc_closure( sp, this->enc_val_ref(this->field(FMT(idx))) ) )
                    ) );
            }
            )
        )

        nodes.push_back( this->get_val_ok(opts.core_name) );
        auto closure = this->enc_closure( sp, NEWNODE(Block, mv$(nodes)) );

        ::AST::ExprNodeP    node;
        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            node = get_val_ok(opts.core_name);
            ),
        (Struct,
            node = NEWNODE(CallPath,
                this->get_trait_path_Encoder() + "emit_struct",
                vec$( NEWNODE(NamedValue, AST::Path("s")), NEWNODE(String, struct_name), NEWNODE(Integer, e.ents.size(), CORETYPE_UINT), mv$(closure) )
                );
            ),
        (Tuple,
            node = NEWNODE(CallPath,
                this->get_trait_path_Encoder() + "emit_tuple_struct",
                vec$( NEWNODE(NamedValue, AST::Path("s")), NEWNODE(String, struct_name), NEWNODE(Integer, e.ents.size(), CORETYPE_UINT), mv$(closure) )
                );
            )
        )

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), mv$(node));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back().args() = ::AST::PathParams();
        ::std::vector<AST::ExprNode_Match_Arm>   arms;

        auto s_ent = NEWNODE(NamedValue, AST::Path("s"));

        for(unsigned int var_idx = 0; var_idx < enm.variants().size(); var_idx ++)
        {
            const auto& v = enm.variants()[var_idx];
            AST::ExprNodeP  code;
            AST::Pattern    pat_a;

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = NEWNODE(CallPath, this->get_trait_path_Encoder() + "emit_enum_variant",
                    vec$(
                        s_ent->clone(),
                        NEWNODE(String, v.m_name.c_str()),
                        NEWNODE(Integer, var_idx, CORETYPE_UINT),
                        NEWNODE(Integer, 0, CORETYPE_UINT),
                        this->enc_closure(sp, this->get_val_ok(opts.core_name))
                        )
                    );
                pat_a = AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Named(base_path + v.m_name));
                ),
            (Tuple,
                ::std::vector<AST::Pattern>    pats_a;
                auto nodes = make_refpat_a(sp, pats_a, e.m_sub_types, [&](size_t idx, auto a){
                    return NEWNODE(CallPath, this->get_trait_path_Encoder() + RcString::new_interned("emit_enum_variant_arg"),
                        vec$(
                            s_ent->clone(),
                            NEWNODE(Integer, idx, CORETYPE_UINT),
                            this->enc_closure(sp, this->enc_val_direct(mv$(a)))
                            )
                        );
                    });
                nodes.push_back( this->get_val_ok(opts.core_name) );

                code = NEWNODE(CallPath, this->get_trait_path_Encoder() + "emit_enum_variant",
                    vec$(
                        s_ent->clone(),
                        NEWNODE(String, v.m_name.c_str()),
                        NEWNODE(Integer, var_idx, CORETYPE_UINT),
                        NEWNODE(Integer, e.m_sub_types.size(), CORETYPE_UINT),
                        this->enc_closure(sp, NEWNODE(Block, mv$(nodes)))
                        )
                    );
                pat_a = AST::Pattern(AST::Pattern::TagNamedTuple(), sp, base_path + v.m_name, mv$(pats_a));
                ),
            (Struct,
                ::std::vector< ::std::pair<RcString, AST::Pattern> > pats_a;
                auto nodes = make_refpat_a(sp, pats_a, e.m_fields, [&](size_t idx, auto a){
                    return NEWNODE(CallPath, this->get_trait_path_Encoder() + RcString::new_interned("emit_enum_variant_arg"),
                        vec$(
                            s_ent->clone(),
                            NEWNODE(String, e.m_fields[idx].m_name.c_str()),
                            NEWNODE(Integer, idx, CORETYPE_UINT),
                            this->enc_closure(sp, this->enc_val_direct(mv$(a)))
                            )
                        );
                    });
                nodes.push_back( this->get_val_ok(opts.core_name) );

                pat_a = AST::Pattern(AST::Pattern::TagStruct(), sp, base_path + v.m_name, mv$(pats_a), true);
                code = NEWNODE(CallPath, this->get_trait_path_Encoder() + "emit_enum_struct_variant",
                    vec$(
                        s_ent->clone(),
                        NEWNODE(String, v.m_name.c_str()),
                        NEWNODE(Integer, var_idx, CORETYPE_UINT),
                        NEWNODE(Integer, e.m_fields.size(), CORETYPE_UINT),
                        this->enc_closure(sp, NEWNODE(Block, mv$(nodes)))
                        )
                    );
                )
            )

            ::std::vector< AST::Pattern>    pats;
            pats.push_back( AST::Pattern(AST::Pattern::TagReference(), sp, false, mv$(pat_a)) );

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                mv$(code)
                ));
        }

        auto node_match = NEWNODE(Match, NEWNODE(NamedValue, AST::Path("self")), mv$(arms));

        ::std::string enum_name = type.m_data.as_Path().path.nodes().back().name().c_str();
        auto node = NEWNODE(CallPath, this->get_trait_path_Encoder() + "emit_enum",
            vec$( mv$(s_ent), NEWNODE(String, enum_name), this->enc_closure(sp, mv$(node_match)) )
            );

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), mv$(node));
    }
} g_derive_rustc_encodable;

class Deriver_RustcDecodable:
    public Deriver
{
    // NOTE: This emits paths like `::rustc_serialize::Encodable` - rustc and crates.io have subtly different crate names
    AST::Path get_trait_path() const {
        return get_path("", "rustc_serialize", "Decodable");
    }
    AST::Path get_trait_path_Decoder() const {
        return get_path("", "rustc_serialize", "Decoder");
    }
    AST::Path get_method_path() const {
        return get_trait_path() + "decode";
    }

    AST::Impl make_ret(Span sp, const RcString& core_name, const AST::GenericParams& p, const TypeRef& type, ::std::vector<TypeRef> types_to_bound, AST::ExprNodeP node) const
    {
        const AST::Path    trait_path = this->get_trait_path();

        AST::Path result_path = get_path(core_name, "result", "Result");
        result_path.nodes()[1].args().m_types.push_back( TypeRef(sp, "Self", 0xFFFF) );
        result_path.nodes()[1].args().m_types.push_back( TypeRef(sp, AST::Path(AST::Path::TagUfcs(), TypeRef(sp, "D", 0x100|0), this->get_trait_path_Decoder(), { AST::PathNode("Error",{}) })) );

        AST::Function fcn(
            sp,
            AST::GenericParams(),
            ABI_RUST, false, false, false,
            TypeRef(sp, result_path),
            vec$(
                //::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "self"), TypeRef(TypeRef::TagReference(), sp, false, AST::LifetimeRef(), TypeRef(sp, "Self", 0xFFFF)) ),
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "d"), TypeRef(TypeRef::TagReference(), sp, AST::LifetimeRef(), true, TypeRef(sp, "D", 0x100|0)) )
                )
            );
        fcn.params().add_ty_param( AST::TypeParam(sp, {}, "D") );
        fcn.params().add_bound( AST::GenericBound::make_IsTrait({
            {}, TypeRef(sp, "D", 0x100|0),
            {}, this->get_trait_path_Decoder()
            }) );
        fcn.set_code( NEWNODE(Block, vec$(mv$(node))) );

        AST::GenericParams  params = get_params_with_bounds(sp, p, trait_path, mv$(types_to_bound));

        AST::Impl   rv( AST::ImplDef( AST::AttributeList(), mv$(params), make_spanned(sp, trait_path), type.clone() ) );
        rv.add_function(false, false, "decode", mv$(fcn));
        return mv$(rv);
    }
    AST::ExprNodeP dec_val() const {
        return NEWNODE(CallPath, this->get_method_path(),  vec$( NEWNODE(NamedValue, AST::Path("d")) ));
    }
    AST::ExprNodeP field(const ::std::string& name) const {
        return NEWNODE(Field, NEWNODE(NamedValue, AST::Path(RcString::new_interned("self"))), RcString::new_interned(name));
    }

    AST::ExprNodeP dec_closure(Span sp, AST::ExprNodeP code) const {
        return NEWNODE(Closure,
            vec$( ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "d"), ::TypeRef(sp) ) ), ::TypeRef(sp),
            mv$(code), false
            );
    }
    AST::ExprNodeP get_val_err_str(const RcString& core_name, ::std::string err_str) const {
        return NEWNODE(CallPath, get_path(core_name, "result", "Result", "Err"), vec$(
            NEWNODE(CallMethod,
                NEWNODE(NamedValue, AST::Path("d")),
                AST::PathNode("error"),
                vec$( NEWNODE(String, err_str) )
                )
            ) );
    }
    AST::ExprNodeP get_val_ok(const RcString& core_name, AST::ExprNodeP inner) const {
        return NEWNODE(CallPath, get_path(core_name, "result", "Result", "Ok"), vec$( mv$(inner) ) );
    }
    AST::ExprNodeP get_val_ok_unit(const RcString& core_name) const {
        return get_val_ok(core_name, NEWNODE(Tuple, {}));
    }

public:
    const char* trait_name() const override { return "RustcDecodable"; }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Struct& str) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        ::std::string struct_name = type.m_data.as_Path().path.nodes().back().name().c_str();

        AST::ExprNodeP  node_v;
        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            ),
        (Struct,
            ::AST::ExprNode_StructLiteral::t_values vals;
            unsigned int idx = 0;
            for( const auto& fld : e.ents )
            {
                vals.push_back({ {}, fld.m_name, NEWNODE(UniOp, ::AST::ExprNode_UniOp::QMARK, NEWNODE(CallPath,
                    this->get_trait_path_Decoder() + "read_struct_field",
                    vec$( NEWNODE(NamedValue, AST::Path("d")), NEWNODE(String, fld.m_name.c_str()), NEWNODE(Integer, idx, CORETYPE_UINT), this->dec_closure( sp, this->dec_val() ) )
                    )) });
                idx ++;
            }
            node_v = NEWNODE(StructLiteral, base_path, nullptr, mv$(vals));
            ),
        (Tuple,
            ::std::vector<AST::ExprNodeP>   vals;
            for( unsigned int idx = 0; idx < e.ents.size(); idx ++ )
            {
                vals.push_back( NEWNODE(UniOp, ::AST::ExprNode_UniOp::QMARK, NEWNODE(CallPath,
                    this->get_trait_path_Decoder() + "read_tuple_struct_arg",
                    vec$( NEWNODE(NamedValue, AST::Path("d")), NEWNODE(Integer, idx, CORETYPE_UINT), this->dec_closure(sp, this->dec_val()) )
                    )) );
            }
            node_v = NEWNODE(CallPath, mv$(base_path), mv$(vals));
            )
        )

        auto closure = this->dec_closure( sp, this->get_val_ok(opts.core_name, mv$(node_v)) );

        auto args = vec$( NEWNODE(NamedValue, AST::Path("d")), NEWNODE(String, struct_name), AST::ExprNodeP(), mv$(closure) );

        ::AST::ExprNodeP    node;
        TU_MATCH(AST::StructData, (str.m_data), (e),
        (Unit,
            node = this->get_val_ok(opts.core_name, NEWNODE(NamedValue, mv$(base_path)));
            ),
        (Struct,
            assert( !args[2] );
            args[2] = NEWNODE(Integer, e.ents.size(), CORETYPE_UINT);
            node = NEWNODE(CallPath, this->get_trait_path_Decoder() + "read_struct", mv$(args) );
            ),
        (Tuple,
            assert( !args[2] );
            args[2] = NEWNODE(Integer, e.ents.size(), CORETYPE_UINT);
            node = NEWNODE(CallPath, this->get_trait_path_Decoder() + "read_tuple_struct", mv$(args) );
            )
        )

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(str), mv$(node));
    }

    AST::Impl handle_item(Span sp, const DeriveOpts& opts, const AST::GenericParams& p, const TypeRef& type, const AST::Enum& enm) const override
    {
        AST::Path base_path = type.m_data.as_Path().path;
        base_path.nodes().back().args() = ::AST::PathParams();
        ::std::vector<AST::ExprNode_Match_Arm>   arms;

        // 1. Variant names
        ::std::vector< AST::ExprNodeP>  var_name_strs;

        // 2. Decoding arms
        for(unsigned int var_idx = 0; var_idx < enm.variants().size(); var_idx ++)
        {
            const auto& v = enm.variants()[var_idx];
            AST::ExprNodeP  code;

            TU_MATCH(::AST::EnumVariantData, (v.m_data), (e),
            (Value,
                code = NEWNODE(NamedValue, base_path + v.m_name);
                ),
            (Tuple,
                ::std::vector<AST::ExprNodeP>   args;

                for( unsigned int idx = 0; idx < e.m_sub_types.size(); idx ++ )
                {
                    args.push_back( NEWNODE(UniOp, ::AST::ExprNode_UniOp::QMARK, NEWNODE(CallPath, this->get_trait_path_Decoder() + "read_enum_variant_arg",
                        vec$(
                            NEWNODE(NamedValue, AST::Path("d")),
                            NEWNODE(Integer, idx, CORETYPE_UINT),
                            this->dec_closure(sp, this->dec_val())
                            )
                        )) );
                }
                code = NEWNODE(CallPath, base_path + v.m_name, mv$(args));
                ),
            (Struct,
                ::AST::ExprNode_StructLiteral::t_values vals;

                unsigned int idx = 0;
                for( const auto& fld : e.m_fields )
                {
                    vals.push_back({ {}, fld.m_name, NEWNODE(UniOp, ::AST::ExprNode_UniOp::QMARK, NEWNODE(CallPath, this->get_trait_path_Decoder() + "read_enum_struct_variant_field",
                        vec$(
                            NEWNODE(NamedValue, AST::Path("d")),
                            NEWNODE(String, fld.m_name.c_str()),
                            NEWNODE(Integer, idx, CORETYPE_UINT),
                            this->dec_closure(sp, this->dec_val())
                            )
                        ) )});
                    idx ++;
                }

                code = NEWNODE(StructLiteral, base_path + v.m_name, nullptr, mv$(vals) );
                )
            )

            ::std::vector< AST::Pattern>    pats;
            pats.push_back( AST::Pattern(AST::Pattern::TagValue(), sp, AST::Pattern::Value::make_Integer({CORETYPE_UINT, var_idx})) );

            arms.push_back(AST::ExprNode_Match_Arm(
                mv$(pats),
                nullptr,
                this->get_val_ok(opts.core_name, mv$(code))
                ));
            var_name_strs.push_back( NEWNODE(String, v.m_name.c_str()) );
        }

        // Default arm
        {
            arms.push_back(AST::ExprNode_Match_Arm(
                ::make_vec1( AST::Pattern() ),
                nullptr,
                this->get_val_err_str(opts.core_name, "enum value unknown")
                ));
        }

        auto node_match = NEWNODE(Match, NEWNODE(NamedValue, AST::Path("idx")), mv$(arms));
        auto node_var_closure = NEWNODE(Closure,
            vec$(
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "d"), ::TypeRef(sp) ),
                ::std::make_pair( AST::Pattern(AST::Pattern::TagBind(), sp, "idx"), ::TypeRef(sp) )
                ),
            ::TypeRef(sp),
            mv$(node_match),
            false
            );
        ::std::string enum_name = type.m_data.as_Path().path.nodes().back().name().c_str();

        auto node_rev = NEWNODE(CallPath, this->get_trait_path_Decoder() + "read_enum_variant",
            vec$(
                NEWNODE(NamedValue, AST::Path("d")),
                NEWNODE(UniOp, AST::ExprNode_UniOp::REF, NEWNODE(Array, mv$(var_name_strs))),
                mv$( node_var_closure )
                )
            );

        auto node = NEWNODE(CallPath, this->get_trait_path_Decoder() + "read_enum",
            vec$( NEWNODE(NamedValue, AST::Path("d")), NEWNODE(String, enum_name), this->dec_closure(sp, mv$(node_rev)) )
            );

        return this->make_ret(sp, opts.core_name, p, type, this->get_field_bounds(enm), mv$(node));
    }
} g_derive_rustc_decodable;


// --------------------------------------------------------------------
// Select and dispatch the correct derive() handler
// --------------------------------------------------------------------
static const Deriver* find_impl(const RcString& trait_name)
{
    #define _(obj)  if(trait_name == obj.trait_name()) return &obj;
    _(g_derive_debug)
    _(g_derive_partialeq)
    _(g_derive_partialord)
    _(g_derive_eq)
    _(g_derive_ord)
    _(g_derive_clone)
    _(g_derive_copy)
    _(g_derive_default)
    _(g_derive_hash)
    _(g_derive_rustc_encodable)
    _(g_derive_rustc_decodable)
    #undef _
    return nullptr;
}

template<typename T>
static void derive_item(const Span& sp, const AST::Crate& crate, AST::Module& mod, const AST::Attribute& attr, const AST::Path& path, const T& item)
{
    if( !attr.has_sub_items() ) {
        //ERROR(sp, E0000, "#[derive()] requires a list of known traits to derive");
        return ;
    }

    DEBUG("path = " << path);
    bool    fail = false;

    const auto& params = item.params();
    TypeRef type(sp, path);
    auto& types_args = type.path().nodes().back().args();
    for( const auto& param : params.ty_params() ) {
        types_args.m_types.push_back( TypeRef(TypeRef::TagArg(), sp, param.name()) );
    }

    DeriveOpts opts = {
        (crate.m_load_std == ::AST::Crate::LOAD_NONE ? "" : "core"),
        attr.items()
        };

    ::std::vector< RcString>   missing_handlers;
    for( const auto& trait : attr.items() )
    {
        DEBUG("- " << trait.name());
        auto dp = find_impl(trait.name());
        if( dp ) {
            mod.add_item(false, "", dp->handle_item(sp, opts, params, type, item), {} );
            continue ;
        }

        // Support custom derive
        auto mac_name = FMT("derive#" << trait.name());
        // - Requires support all through the chain.
        bool found = false;
        for(const auto& mac_path : mod.m_macro_imports)
        {
            if( mac_path.name == mac_name )
            {
                if( mac_path.macro_ptr ) {
                    // macro_rules! based derive?
                    TODO(sp, "Custom derive using macro_rules?");
                }
                else {
                    // proc_macro - Invoke the handler.
                    auto lex = ProcMacro_Invoke(sp, crate, mac_path.path, path.nodes().back().name().c_str(), item);
                    if( lex )
                    {
                        Parse_ModRoot_Items(*lex, mod);
                        found = true;
                    }
                    else {
                        ERROR(sp, E0000, "proc_macro derive failed");
                    }
                    break;
                }
            }
        }
        if( found )
            continue ;

        DEBUG("> No handler for " << trait.name());
        missing_handlers.push_back( trait.name() );
        fail = true;
    }

    if( fail ) {
        ERROR(sp, E0000, "Failed to apply #[derive] - Missing handlers for " << missing_handlers);
    }
}

class Decorator_Derive:
    public ExpandDecorator
{
public:
    AttrStage stage() const override { return AttrStage::Post; }
    void handle(const Span& sp, const AST::Attribute& attr, ::AST::Crate& crate, const AST::Path& path, AST::Module& mod, AST::Item& i) const override
    {
        TU_MATCH_DEF(::AST::Item, (i), (e),
        (
            TODO(sp, "Handle #[derive] for other item types - " << i.tag_str());
            ),
        (None,
            // Ignore, it's been deleted
            ),
        (Union,
            derive_item(sp, crate, mod, attr, path, e);
            ),
        (Enum,
            derive_item(sp, crate, mod, attr, path, e);
            ),
        (Struct,
            derive_item(sp, crate, mod, attr, path, e);
            )
        )
    }
};

STATIC_DECORATOR("derive", Decorator_Derive)

