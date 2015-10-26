#include <string>
#include <vector>
#include "isaac/kernels/mapped_object.h"
#include "isaac/kernels/parse.h"

namespace isaac
{

namespace templates
{

class map_functor : public traversal_functor
{

  numeric_type get_numeric_type(isaac::math_expression const * math_expression, size_t root_idx) const
{
  math_expression::node const * root_node = &math_expression->tree()[root_idx];
  while (root_node->lhs.dtype==INVALID_NUMERIC_TYPE)
    root_node = &math_expression->tree()[root_node->lhs.node_index];
  return root_node->lhs.dtype;
}

  template<class T>
  std::shared_ptr<mapped_object> binary_leaf(isaac::math_expression const * math_expression, size_t root_idx, mapping_type const * mapping) const
  {
    return std::shared_ptr<mapped_object>(new T(to_string(math_expression->dtype()), binder_.get(), mapped_object::node_info(mapping, math_expression, root_idx)));
  }

  std::shared_ptr<mapped_object> create(numeric_type dtype, values_holder) const
  {
    std::string strdtype = to_string(dtype);
    return std::shared_ptr<mapped_object>(new mapped_host_scalar(strdtype, binder_.get()));
  }

  std::shared_ptr<mapped_object> create(array_base const * a, bool is_assigned)  const
  {
    std::string dtype = to_string(a->dtype());
    unsigned int id = binder_.get(a, is_assigned);
    //Scalar
    if(max(a->shape())==1)
      return std::shared_ptr<mapped_object>(new mapped_array(dtype, id, 's'));
    //Vector
    else if(a->dim()==1){
      return std::shared_ptr<mapped_object>(new mapped_array(dtype, id, 'c'));
    }
    //Matrix
    else
      return std::shared_ptr<mapped_object>(new mapped_array(dtype, id, 'm'));
  }

  std::shared_ptr<mapped_object> create(lhs_rhs_element const & lhs_rhs, bool is_assigned = false) const
  {
    switch(lhs_rhs.type_family)
    {
      case VALUE_TYPE_FAMILY: return create(lhs_rhs.dtype, lhs_rhs.vscalar);
      case ARRAY_TYPE_FAMILY: return create(lhs_rhs.array, is_assigned);
      case PLACEHOLDER_TYPE_FAMILY: return std::shared_ptr<mapped_object>(new mapped_placeholder(lhs_rhs.for_idx.level));
      default: throw "";
    }
  }


public:
  map_functor(symbolic_binder & binder, mapping_type & mapping, const driver::Device &device)
      : binder_(binder), mapping_(mapping), device_(device)
  {
  }

  void operator()(isaac::math_expression const & math_expression, size_t root_idx, leaf_t leaf_t) const
  {
    mapping_type::key_type key(root_idx, leaf_t);
    math_expression::node const & root_node = math_expression.tree()[root_idx];

    if (leaf_t == LHS_NODE_TYPE && root_node.lhs.type_family != COMPOSITE_OPERATOR_FAMILY)
      mapping_.insert(mapping_type::value_type(key, create(root_node.lhs, detail::is_assignment(root_node.op))));
    else if (leaf_t == RHS_NODE_TYPE && root_node.rhs.type_family != COMPOSITE_OPERATOR_FAMILY)
      mapping_.insert(mapping_type::value_type(key, create(root_node.rhs)));
    else if ( leaf_t== PARENT_NODE_TYPE)
    {
      if (root_node.op.type==OPERATOR_VDIAG_TYPE)
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_vdiag>(&math_expression, root_idx, &mapping_)));
      else if (root_node.op.type==OPERATOR_MATRIX_DIAG_TYPE)
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_matrix_diag>(&math_expression, root_idx, &mapping_)));
      else if (root_node.op.type==OPERATOR_MATRIX_ROW_TYPE)
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_matrix_row>(&math_expression, root_idx, &mapping_)));
      else if (root_node.op.type==OPERATOR_MATRIX_COLUMN_TYPE)
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_matrix_column>(&math_expression, root_idx, &mapping_)));
      else if(root_node.op.type==OPERATOR_ACCESS_INDEX_TYPE)
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_array_access>(&math_expression, root_idx, &mapping_)));
      else if (detail::is_scalar_dot(root_node))
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_scalar_dot>(&math_expression, root_idx, &mapping_)));
      else if (detail::is_vector_dot(root_node))
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_gemv>(&math_expression, root_idx, &mapping_)));
      else if (root_node.op.type_family == OPERATOR_GEMM_TYPE_FAMILY)
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_gemm>(&math_expression, root_idx, &mapping_)));
      else if (root_node.op.type == OPERATOR_REPEAT_TYPE)
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_repeat>(&math_expression, root_idx, &mapping_)));
      else if (root_node.op.type == OPERATOR_OUTER_PROD_TYPE)
        mapping_.insert(mapping_type::value_type(key, binary_leaf<mapped_outer>(&math_expression, root_idx, &mapping_)));
      else if (detail::is_cast(root_node.op))
        mapping_.insert(mapping_type::value_type(key, std::shared_ptr<mapped_object>(new mapped_cast(root_node.op.type, binder_.get()))));
    }
  }
private:
  symbolic_binder & binder_;
  mapping_type & mapping_;
  driver::Device const & device_;
};


}

}
