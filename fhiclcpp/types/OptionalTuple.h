#ifndef fhiclcpp_types_OptionalTuple_h
#define fhiclcpp_types_OptionalTuple_h

#include "fhiclcpp/detail/printing_helpers.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/detail/NameStackRegistry.h"
#include "fhiclcpp/types/detail/TableMemberRegistry.h"
#include "fhiclcpp/types/detail/SequenceBase.h"
#include "fhiclcpp/types/detail/type_traits_error_msgs.h"
#include "fhiclcpp/type_traits.h"

#include <string>
#include <utility>

namespace fhicl {

  class ParameterSet;

  //==================================================================
  // e.g. OptionalTuple<int,double,bool> ====> std::tuple<int,double,bool>
  //

  template<typename ... TYPES>
  class OptionalTuple final :
    public  detail::SequenceBase,
    private detail::RegisterIfTableMember {
  public:

    using ftype = std::tuple< std::shared_ptr< tt::fhicl_type<TYPES> >... >;
    using rtype = std::tuple< tt::return_type<TYPES>... >;

    explicit OptionalTuple(Name&& name, Comment&& comment );
    explicit OptionalTuple(Name&& name) : OptionalTuple( std::move(name), Comment("") ) {}

    bool operator()(rtype&) const;

  private:

    ftype value_;
    bool  has_value_ { false };

    std::size_t get_size() const override { return std::tuple_size<ftype>(); }

    //===================================================================
    // iterate over tuple elements
    using PW_non_const = detail::ParameterWalker<tt::const_flavor::require_non_const>;
    using PW_const     = detail::ParameterWalker<tt::const_flavor::require_const>;

    void visit_element(PW_non_const&){}

    template <typename E, typename ... T>
    void visit_element(PW_non_const& pw, E& elem, T& ... others)
    {
      using elem_ftype = typename E::element_type;
      static_assert(!tt::is_table_fragment<elem_ftype>::value, NO_NESTED_TABLE_FRAGMENTS);
      static_assert(!tt::is_optional_parameter<elem_ftype>::value, NO_OPTIONAL_TYPES );
      pw(*elem);
      visit_element(pw, others...);
    }

    template <std::size_t ... I>
    void iterate_over_tuple(PW_non_const& pw, std::index_sequence<I...>)
    {
      visit_element(pw, std::get<I>(value_)...);
    }

    void do_walk_elements(PW_non_const& pw)
    {
      iterate_over_tuple(pw, std::index_sequence_for<TYPES...>{});
    }

    void visit_element(PW_const&) const {}

    template <typename E, typename ... T>
    void visit_element(PW_const& pw, E const& elem, T const& ... others) const
    {
      using elem_ftype = typename E::element_type;
      static_assert(!tt::is_table_fragment<elem_ftype>::value, NO_NESTED_TABLE_FRAGMENTS);
      static_assert(!tt::is_optional_parameter<elem_ftype>::value, NO_OPTIONAL_TYPES );
      pw(*elem);
      visit_element(pw, others...);
    }

    template <std::size_t ... I>
    void iterate_over_tuple(PW_const& pw, std::index_sequence<I...>) const
    {
      visit_element(pw, std::get<I>(value_)...);
    }

    void do_walk_elements(PW_const& pw) const
    {
      iterate_over_tuple(pw, std::index_sequence_for<TYPES...>{});
    }

    //===================================================================
    // finalizing tuple elements
    void finalize_tuple_elements(std::size_t){}

    // 'E' and 'T' are shared_ptr's.
    template <typename E, typename ... T>
    void finalize_tuple_elements(std::size_t i, E& elem, T& ... others)
    {
      using elem_ftype = typename E::element_type;
      static_assert(!tt::is_table_fragment<elem_ftype>::value, NO_NESTED_TABLE_FRAGMENTS);
      static_assert(!tt::is_optional_parameter<elem_ftype>::value, NO_OPTIONAL_TYPES );

      elem = std::make_shared<elem_ftype>( Name::sequence_element(i) );
      finalize_tuple_elements(++i, others...);
    }

    template <std::size_t ... I>
    void finalize_elements(std::index_sequence<I...>)
    {
      finalize_tuple_elements(0, std::get<I>(value_)...);
    }

    //===================================================================
    // filling return type

    using TUPLE  = std::tuple<tt::fhicl_type<TYPES>...>;

    template <size_t I, typename rtype>
    std::enable_if_t<(I >= std::tuple_size<TUPLE>::value)>
    fill_return_element(rtype &) const
    {}

    template <size_t I, typename rtype>
    std::enable_if_t<(I < std::tuple_size<TUPLE>::value)>
    fill_return_element(rtype & result) const
    {
      std::get<I>(result) = (*std::get<I>(value_))();
      fill_return_element<I+1>(result);
    }

    void assemble_rtype(rtype & result) const
    {
      fill_return_element<0>( result );
    }

    void do_set_value(fhicl::ParameterSet const&, bool const /*trimParents*/) override
    {
      // We do not explicitly set the sequence values here as the
      // individual elements are set one at a time.  However, this
      // function is reached in the ValidateThenSet algorithm if the
      // optional parameter is present.  Otherwise, this override is
      // skipped.
      has_value_ = true;
    }

  }; // class OptionalTuple

  //================= IMPLEMENTATION =========================
  //
  template<typename ... TYPES>
  OptionalTuple<TYPES...>::OptionalTuple(Name&& name,
                                         Comment&& comment)
    : SequenceBase{std::move(name), std::move(comment), value_type::OPTIONAL, par_type::TUPLE}
    , RegisterIfTableMember{this}
  {
    finalize_elements(std::index_sequence_for<TYPES...>{});
    NameStackRegistry::end_of_ctor();
  }

  template<typename ... TYPES>
  bool
  OptionalTuple<TYPES...>::operator()(rtype& r) const
  {
    if (!has_value_) return false;
    rtype result;
    assemble_rtype(result);
    std::swap(result, r);
    return true;
  }

}

#endif

// Local variables:
// mode: c++
// End:
