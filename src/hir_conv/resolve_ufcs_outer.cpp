/*
 * MRustC - Rust Compiler
 * - By John Hodge (Mutabah/thePowersGang)
 *
 * hir_conv/resolve_ufcs_outer.cpp
 * - Resolve UfcsUnknown paths in outer scope (signatures/types)
 *
 * RULES:
 * - Only generics are allowed to be UfcsKnown in signatures/types (within bodies anything goes?)
 */
#include "main_bindings.hpp"
#include <hir/hir.hpp>
#include <hir/expr.hpp>
#include <hir/visitor.hpp>
#include <hir_typeck/static.hpp>
#include <hir_typeck/common.hpp>    // monomorphise_genericpath_needed
#include <algorithm>

namespace {
    class Visitor:
        public ::HIR::Visitor
    {
        const ::HIR::Crate& m_crate;

        StaticTraitResolve  m_resolve;
        const ::HIR::TypeRef* m_current_type = nullptr; // used because sometimes `Self` is encoded expanded (is this a problem?)
        const ::HIR::Trait* m_current_trait = nullptr;
        const ::HIR::ItemPath* m_current_trait_path = nullptr;

    public:
        Visitor(const ::HIR::Crate& crate):
            m_crate(crate),
            m_resolve(crate)
        {}

        void visit_struct(::HIR::ItemPath p, ::HIR::Struct& item) override {
            auto _ = m_resolve.set_item_generics(item.m_params);
            ::HIR::Visitor::visit_struct(p, item);
        }
        void visit_enum(::HIR::ItemPath p, ::HIR::Enum& item) override {
            auto _ = m_resolve.set_item_generics(item.m_params);
            ::HIR::Visitor::visit_enum(p, item);
        }
        void visit_function(::HIR::ItemPath p, ::HIR::Function& item) override {
            auto _ = m_resolve.set_item_generics(item.m_params);
            ::HIR::Visitor::visit_function(p, item);
        }
        void visit_type_alias(::HIR::ItemPath p, ::HIR::TypeAlias& item) override {
            // NOTE: Disabled, becuase generics in type aliases are never checked
#if 0
            auto _ = m_resolve.set_item_generics(item.m_params);
            ::HIR::Visitor::visit_type_alias(p, item);
#endif
        }
        void visit_trait(::HIR::ItemPath p, ::HIR::Trait& trait) override {
            m_current_trait = &trait;
            m_current_trait_path = &p;
            auto _ = m_resolve.set_impl_generics(trait.m_params);
            ::HIR::Visitor::visit_trait(p, trait);
            m_current_trait = nullptr;
        }
        void visit_type_impl(::HIR::TypeImpl& impl) override {
            TRACE_FUNCTION_F("impl" << impl.m_params.fmt_args() << " " << impl.m_type << " (mod=" << impl.m_src_module << ")");
            auto _g = m_resolve.set_impl_generics(impl.m_params);
            m_current_type = &impl.m_type;
            ::HIR::Visitor::visit_type_impl(impl);
            m_current_type = nullptr;
        }
        void visit_marker_impl(const ::HIR::SimplePath& trait_path, ::HIR::MarkerImpl& impl) override {
            ::HIR::ItemPath    p( impl.m_type, trait_path, impl.m_trait_args );
            TRACE_FUNCTION_F("impl" << impl.m_params.fmt_args() << " " << trait_path << impl.m_trait_args << " for " << impl.m_type << " (mod=" << impl.m_src_module << ")");

            auto _g = m_resolve.set_impl_generics(impl.m_params);
            m_current_type = &impl.m_type;
            m_current_trait = &m_crate.get_trait_by_path(Span(), trait_path);
            m_current_trait_path = &p;

            ::HIR::Visitor::visit_marker_impl(trait_path, impl);

            m_current_trait = nullptr;
            m_current_type = nullptr;
        }
        void visit_trait_impl(const ::HIR::SimplePath& trait_path, ::HIR::TraitImpl& impl) override {
            ::HIR::ItemPath    p( impl.m_type, trait_path, impl.m_trait_args );
            TRACE_FUNCTION_F("impl" << impl.m_params.fmt_args() << " " << trait_path << impl.m_trait_args << " for " << impl.m_type << " (mod=" << impl.m_src_module << ")");

            auto _g = m_resolve.set_impl_generics(impl.m_params);
            m_current_type = &impl.m_type;
            m_current_trait = &m_crate.get_trait_by_path(Span(), trait_path);
            m_current_trait_path = &p;

            ::HIR::Visitor::visit_trait_impl(trait_path, impl);

            m_current_trait = nullptr;
            m_current_type = nullptr;
        }

        void visit_expr(::HIR::ExprPtr& expr) override
        {
#if 0
            struct ExprVisitor:
                public ::HIR::ExprVisitorDef
            {
                Visitor& upper_visitor;

                ExprVisitor(Visitor& uv):
                    upper_visitor(uv)
                {}

                void visit(::HIR::ExprNode_Let& node) override
                {
                    upper_visitor.visit_pattern(node.m_pattern);
                    upper_visitor.visit_type(node.m_type);
                    ::HIR::ExprVisitorDef::visit(node);
                }
                void visit(::HIR::ExprNode_Cast& node) override
                {
                    upper_visitor.visit_type(node.m_res_type);
                    ::HIR::ExprVisitorDef::visit(node);
                }

                void visit(::HIR::ExprNode_CallPath& node) override
                {
                    upper_visitor.visit_path(node.m_path, ::HIR::Visitor::PathContext::VALUE);
                    ::HIR::ExprVisitorDef::visit(node);
                }
                void visit(::HIR::ExprNode_CallMethod& node) override
                {
                    upper_visitor.visit_path_params(node.m_params);
                    ::HIR::ExprVisitorDef::visit(node);
                }

                void visit(::HIR::ExprNode_ArraySized& node) override
                {
                    upper_visitor.visit_expr(node.m_size);
                    ::HIR::ExprVisitorDef::visit(node);
                }

                void visit(::HIR::ExprNode_PathValue& node) override
                {
                    upper_visitor.visit_path(node.m_path, ::HIR::Visitor::PathContext::VALUE);
                    ::HIR::ExprVisitorDef::visit(node);
                }
                void visit(::HIR::ExprNode_StructLiteral& node) override
                {
                    upper_visitor.visit_type(node.m_type);
                    ::HIR::ExprVisitorDef::visit(node);
                }

                void visit(::HIR::ExprNode_Match& node) override
                {
                    for(auto& arm : node.m_arms)
                    {
                        for(auto& pat : arm.m_patterns)
                            upper_visitor.visit_pattern(pat);
                    }
                    ::HIR::ExprVisitorDef::visit(node);
                }

                void visit(::HIR::ExprNode_Closure& node) override
                {
                    upper_visitor.visit_type(node.m_return);
                    for(auto& arg : node.m_args) {
                        upper_visitor.visit_pattern(arg.first);
                        upper_visitor.visit_type(arg.second);
                    }
                    ::HIR::ExprVisitorDef::visit(node);
                }

                void visit(::HIR::ExprNode_Block& node) override
                {
                    if( node.m_traits.size() == 0 && node.m_local_mod.m_components.size() > 0 ) {
                        const auto& mod = upper_visitor.m_crate.get_mod_by_path(node.span(), node.m_local_mod);
                        for( const auto& trait_path : mod.m_traits ) {
                            node.m_traits.push_back( ::std::make_pair( &trait_path, &upper_visitor.m_crate.get_trait_by_path(node.span(), trait_path) ) );
                        }
                    }
                    for( const auto& trait_ref : node.m_traits )
                        upper_visitor.m_traits.push_back( trait_ref );
                    ::HIR::ExprVisitorDef::visit(node);
                    for(unsigned int i = 0; i < node.m_traits.size(); i ++ )
                        upper_visitor.m_traits.pop_back();
                }
            };

            if( expr.get() != nullptr )
            {
                m_in_expr = true;
                ExprVisitor v { *this };
                (*expr).visit(v);
                m_in_expr = false;
            }
#endif
        }

        bool locate_trait_item_in_bounds(::HIR::Visitor::PathContext pc,  const ::HIR::TypeRef& tr, const ::HIR::GenericParams& params,  ::HIR::Path::Data& pd) {
            static Span sp;
            //const auto& name = pd.as_UfcsUnknown().item;
            for(const auto& b : params.m_bounds)
            {
                TU_IFLET(::HIR::GenericBound, b, TraitBound, e,
                    DEBUG("- " << e.type << " : " << e.trait.m_path);
                    if( e.type == tr ) {
                        DEBUG(" - Match");
                        if( locate_in_trait_and_set(pc, e.trait.m_path, m_crate.get_trait_by_path(sp, e.trait.m_path.m_path),  pd) ) {
                            return true;
                        }
                    }
                );
                // -
            }
            return false;
        }
        static ::HIR::Path::Data get_ufcs_known(::HIR::Path::Data::Data_UfcsUnknown e,  ::HIR::GenericPath trait_path, const ::HIR::Trait& trait)
        {
            return ::HIR::Path::Data::make_UfcsKnown({ mv$(e.type), mv$(trait_path), mv$(e.item), mv$(e.params)} );
        }
        static bool locate_item_in_trait(::HIR::Visitor::PathContext pc, const ::HIR::Trait& trait,  ::HIR::Path::Data& pd)
        {
            const auto& e = pd.as_UfcsUnknown();

            switch(pc)
            {
            case ::HIR::Visitor::PathContext::VALUE:
                if( trait.m_values.find( e.item ) != trait.m_values.end() ) {
                    return true;
                }
                break;
            case ::HIR::Visitor::PathContext::TRAIT:
                break;
            case ::HIR::Visitor::PathContext::TYPE:
                if( trait.m_types.find( e.item ) != trait.m_types.end() ) {
                    return true;
                }
                break;
            }
            return false;
        }
        static ::HIR::GenericPath make_generic_path(::HIR::SimplePath sp, const ::HIR::Trait& trait)
        {
            auto trait_path_g = ::HIR::GenericPath( mv$(sp) );
            for(unsigned int i = 0; i < trait.m_params.m_types.size(); i ++ ) {
                //trait_path_g.m_params.m_types.push_back( ::HIR::TypeRef(trait.m_params.m_types[i].m_name, i) );
                //trait_path_g.m_params.m_types.push_back( ::HIR::TypeRef() );
                trait_path_g.m_params.m_types.push_back( trait.m_params.m_types[i].m_default.clone() );
            }
            return trait_path_g;
        }
        // Locate the item in `pd` and set `pd` to UfcsResolved if found
        // TODO: This code may end up generating paths without the type information they should contain
        bool locate_in_trait_and_set(::HIR::Visitor::PathContext pc, const ::HIR::GenericPath& trait_path, const ::HIR::Trait& trait,  ::HIR::Path::Data& pd) {
            // TODO: Get the span from caller
            static Span _sp;
            const auto& sp = _sp;
            if( locate_item_in_trait(pc, trait,  pd) ) {
                pd = get_ufcs_known(mv$(pd.as_UfcsUnknown()), trait_path.clone() /*make_generic_path(trait_path.m_path, trait)*/, trait);
                return true;
            }

            auto monomorph_cb = [&](const auto& ty)->const ::HIR::TypeRef& {
                const auto& ge = ty.m_data.as_Generic();
                if( ge.binding == 0xFFFF ) {
                    // TODO: This has to be the _exact_ same type, including future ivars.
                    return *pd.as_UfcsUnknown().type;
                }
                else if( (ge.binding >> 8) == 0 ) {
                    auto idx = ge.binding & 0xFF;
                    ASSERT_BUG(sp, idx < trait.m_params.m_types.size(), "");
                    if( idx < trait_path.m_params.m_types.size() )
                        return trait_path.m_params.m_types[idx];
                    // If the param is omitted, but has a default, use the default.
                    else if( trait.m_params.m_types[idx].m_default != ::HIR::TypeRef() ) {
                        const auto& def = trait.m_params.m_types[idx].m_default;
                        if( ! monomorphise_type_needed(def) )
                            return def;
                        if( def == ::HIR::TypeRef("Self", 0xFFFF) )
                            // TODO: This has to be the _exact_ same type, including future ivars.
                            return *pd.as_UfcsUnknown().type;
                        TODO(sp, "Monomorphise default arg " << def << " for trait path " << trait_path);
                    }
                    else
                        BUG(sp, "Binding out of range in " << ty << " for trait path " << trait_path);
                }
                else {
                    ERROR(sp, E0000, "Unexpected generic binding " << ty);
                }
                };
            ::HIR::GenericPath  par_trait_path_tmp;
            auto monomorph_gp_if_needed = [&](const ::HIR::GenericPath& tpl)->const ::HIR::GenericPath& {
                // NOTE: This doesn't monomorph if the parameter set is the same
                if( monomorphise_genericpath_needed(tpl) && tpl.m_params != trait_path.m_params ) {
                    DEBUG("- Monomorph " << tpl);
                    return par_trait_path_tmp = monomorphise_genericpath_with(sp, tpl, monomorph_cb, false /*no infer*/);
                }
                else {
                    return tpl;
                }
                };

            // Search supertraits (recursively)
            for(const auto& pt : trait.m_parent_traits)
            {
                const auto& par_trait_path = monomorph_gp_if_needed(pt.m_path);
                DEBUG("- Check " << par_trait_path);
                if( locate_in_trait_and_set(pc, par_trait_path, *pt.m_trait_ptr,  pd) ) {
                    return true;
                }
            }
            for(const auto& pt : trait.m_all_parent_traits)
            {
                const auto& par_trait_path = monomorph_gp_if_needed(pt.m_path);
                DEBUG("- Check (all) " << par_trait_path);
                if( locate_item_in_trait(pc, *pt.m_trait_ptr,  pd) ) {
                    // TODO: Don't clone if this is from the temp.
                    pd = get_ufcs_known(mv$(pd.as_UfcsUnknown()), par_trait_path.clone(), *pt.m_trait_ptr);
                    return true;
                }
            }
            return false;
        }

        bool resolve_UfcsUnknown_inherent(const ::HIR::Path& p, ::HIR::Visitor::PathContext pc, ::HIR::Path::Data& pd)
        {
            auto& e = pd.as_UfcsUnknown();
            return m_crate.find_type_impls(*e.type, [&](const auto& t)->const auto& { return t; }, [&](const auto& impl) {
                DEBUG("- matched inherent impl" << impl.m_params.fmt_args() << " " << impl.m_type);
                // Search for item in this block
                switch( pc )
                {
                case ::HIR::Visitor::PathContext::VALUE:
                    if( impl.m_methods.find(e.item) != impl.m_methods.end() ) {
                    }
                    else if( impl.m_constants.find(e.item) != impl.m_constants.end() ) {
                    }
                    else {
                        return false;
                    }
                    // Found it, just keep going (don't care about details here)
                    break;
                case ::HIR::Visitor::PathContext::TRAIT:
                case ::HIR::Visitor::PathContext::TYPE:
                    return false;
                }

                auto new_data = ::HIR::Path::Data::make_UfcsInherent({ mv$(e.type), mv$(e.item), mv$(e.params)} );
                pd = mv$(new_data);
                DEBUG("- Resolved, replace with " << p);
                return true;
                });
        }


        void visit_type(::HIR::TypeRef& ty) override
        {
#if 0
            // TODO: Add a span parameter.
            static Span sp;
#endif

            ::HIR::Visitor::visit_type(ty);

#if 0
            // TODO: If this an associated type, check for default trait params

            unsigned counter = 0;
            while( m_resolve.expand_associated_types_single(sp, ty) )
            {
                ASSERT_BUG(sp, counter++ < 20, "Sanity limit exceeded when resolving UFCS in type " << ty);
                // Invoke a special version of EAT that only processes a single item.
                // - Keep recursing while this does replacements
                ::HIR::Visitor::visit_type(ty);
            }
#endif
        }

        void visit_path(::HIR::Path& p, ::HIR::Visitor::PathContext pc) override
        {
            static Span sp;

            // Explicitly handle UfcsUnknown (doesn't call default)
            if(auto* pe = p.m_data.opt_UfcsUnknown())
            {
                auto& e = *pe;
                TRACE_FUNCTION_FR("UfcsUnknown - p=" << p, p);

                this->visit_type( *e.type );
                this->visit_path_params( e.params );

                // If processing a trait, and the type is 'Self', search for the type/method on the trait
                // - Explicitly encoded because `Self::Type` has a different meaning to `MyType::Type` (the latter will search bounds first)
                if( *e.type == ::HIR::TypeRef("Self", 0xFFFF) )
                {
                    ::HIR::GenericPath  trait_path;
                    if( m_current_trait_path->trait_path() )
                    {
                        trait_path = ::HIR::GenericPath( *m_current_trait_path->trait_path() );
                        trait_path.m_params = m_current_trait_path->trait_args()->clone();
                    }
                    else
                    {
                        trait_path = ::HIR::GenericPath( m_current_trait_path->get_simple_path() );
                        for(unsigned int i = 0; i < m_current_trait->m_params.m_types.size(); i ++ ) {
                            trait_path.m_params.m_types.push_back( ::HIR::TypeRef(m_current_trait->m_params.m_types[i].m_name, i) );
                        }
                    }
                    if( locate_in_trait_and_set(pc, trait_path, *m_current_trait,  p.m_data) ) {
                        DEBUG("Found in Self, p = " << p);
                        return ;
                    }
                    DEBUG("- Item " << e.item << " not found in Self - ty=" << *e.type);
                }

                // Search for matching impls in current generic blocks
                if( m_resolve.m_item_generics != nullptr && locate_trait_item_in_bounds(pc, *e.type, *m_resolve.m_item_generics,  p.m_data) ) {
                    DEBUG("Found in item params, p = " << p);
                    return ;
                }
                if( m_resolve.m_impl_generics != nullptr && locate_trait_item_in_bounds(pc, *e.type, *m_resolve.m_impl_generics,  p.m_data) ) {
                    DEBUG("Found in impl params, p = " << p);
                    return ;
                }


                // If the type is the impl type, look for items AFTER generic lookup
                // TODO: Should this look up in-scope traits instead of hard-coding this hack?
                if( m_current_type && *e.type == *m_current_type )
                {
                    ::HIR::GenericPath  trait_path;
                    if( m_current_trait_path->trait_path() )
                    {
                        trait_path = ::HIR::GenericPath( *m_current_trait_path->trait_path() );
                        trait_path.m_params = m_current_trait_path->trait_args()->clone();
                    }
                    else
                    {
                        trait_path = ::HIR::GenericPath( m_current_trait_path->get_simple_path() );
                        for(unsigned int i = 0; i < m_current_trait->m_params.m_types.size(); i ++ ) {
                            trait_path.m_params.m_types.push_back( ::HIR::TypeRef(m_current_trait->m_params.m_types[i].m_name, i) );
                        }
                    }
                    if( locate_in_trait_and_set(pc, trait_path, *m_current_trait,  p.m_data) ) {
                        DEBUG("Found in Self, p = " << p);
                        return ;
                    }
                    DEBUG("- Item " << e.item << " not found in Self - ty=" << *e.type);
                }

                // TODO: Search all impls of in-scope traits for this method on this type?
                //if( this->resolve_UfcsUnknown_trait(p, pc, p.m_data) ) {
                //    return ;
                //}
                assert(p.m_data.is_UfcsUnknown());

                // Couldn't find it
                ERROR(sp, E0000, "Failed to find impl with '" << e.item << "' for " << *e.type << " (in " << p << ")");
            }
            else
            {
                ::HIR::Visitor::visit_path(p, pc);
            }
        }
    };

    template<typename T>
    void sort_impl_group(::HIR::Crate::ImplGroup<T>& ig)
    {
        auto new_end = ::std::remove_if(ig.generic.begin(), ig.generic.end(), [&ig](::std::unique_ptr<T>& ty_impl) {
            const auto& type = ty_impl->m_type;  // Using field accesses in templates feels so dirty
            const ::HIR::SimplePath*    path = type.get_sort_path();

            if( path )
            {
                ig.named[*path].push_back(mv$(ty_impl));
            }
            else if( type.m_data.is_Path() || type.m_data.is_Generic() )
            {
                return false;
            }
            else
            {
                ig.non_named.push_back(mv$(ty_impl));
            }
            return true;
            });
        ig.generic.erase(new_end, ig.generic.end());
    }
}   // namespace ""

#if 0
void ConvertHIR_ResolveUFCS_Outer(::HIR::Crate& crate)
{
    Visitor exp { crate };
    exp.visit_crate( crate );

    // Sort impls!
    sort_impl_group(crate.m_type_impls);
    DEBUG("Type impl counts: " << crate.m_type_impls.named.size() << " path groups, " << crate.m_type_impls.non_named.size() << " primitive, " << crate.m_type_impls.generic.size() << " ungrouped");
    for(auto& impl_group : crate.m_trait_impls)
    {
        sort_impl_group(impl_group.second);
    }
    for(auto& impl_group : crate.m_marker_impls)
    {
        sort_impl_group(impl_group.second);
    }
}
#endif