#include <cassert>
#include <iostream>
#include <set>
#include <string>

#include "isaac/tuple.h"
#include "isaac/kernels/mapped_object.h"
#include "isaac/kernels/parse.h"
#include "isaac/kernels/stream.h"
#include "isaac/symbolic/expression.h"

#include "find_and_replace.hpp"
#include "to_string.hpp"

namespace isaac
{

void mapped_object::preprocess(std::string &) const { }

void mapped_object::postprocess(std::string &) const { }

void mapped_object::replace_macro(std::string & str, std::string const & macro, MorphBase const & morph)
{
  size_t pos = 0;
  while ((pos=str.find(macro, pos))!=std::string::npos)
  {
    std::string postprocessed;
    size_t pos_po = str.find('{', pos);
    size_t pos_pe = str.find('}', pos_po);

    size_t pos_comma = str.find(',', pos_po);
    if(pos_comma > pos_pe)
    {
        std::string i = str.substr(pos_po + 1, pos_pe - pos_po - 1);
        postprocessed = morph(i);
    }
    else
    {
        std::string i = str.substr(pos_po + 1, pos_comma - pos_po - 1);
        std::string j = str.substr(pos_comma + 1, pos_pe - pos_comma - 1);
        postprocessed = morph(i, j);
    }
    str.replace(pos, pos_pe + 1 - pos, postprocessed);
    pos = pos_pe;
  }
}

void mapped_object::register_attribute(std::string & attribute, std::string const & key, std::string const & value)
{
  attribute = value;
  keywords_[key] = attribute;
}

mapped_object::node_info::node_info(mapping_type const * _mapping, isaac::math_expression const * _math_expression, size_t _root_idx) :
    mapping(_mapping), math_expression(_math_expression), root_idx(_root_idx) { }

mapped_object::mapped_object(std::string const & scalartype, std::string const & name, std::string const & type_key) : type_key_(type_key), name_(name)
{
  register_attribute(scalartype_, "#scalartype", scalartype);
  keywords_["#name"] = name;
}

mapped_object::mapped_object(std::string const & scalartype, unsigned int id, std::string const & type_key) : mapped_object(scalartype, "obj" + tools::to_string(id), type_key)
{ }

mapped_object::~mapped_object()
{ }

std::string mapped_object::type_key() const
{ return type_key_; }

std::string const & mapped_object::name() const
{ return name_; }

std::map<std::string, std::string> const & mapped_object::keywords() const
{ return keywords_; }

std::string mapped_object::process(std::string const & in) const
{
  std::string res(in);
  preprocess(res);
  for (const auto & elem : keywords_)
    tools::find_and_replace(res, elem.first, elem.second);
  postprocess(res);
  return res;
}

std::string mapped_object::evaluate(std::map<std::string, std::string> const & accessors) const
{
  if (accessors.find(type_key_)==accessors.end())
    return name_;
  return process(accessors.at(type_key_));
}

mapped_object& get(math_expression::container_type const & tree, size_t root, mapping_type const & mapping, size_t idx)
{
  for(unsigned int i = 0 ; i < idx ; ++i){
      math_expression::node node = tree[root];
      if(node.rhs.type_family==COMPOSITE_OPERATOR_FAMILY)
        root = node.rhs.node_index;
      else
        return *(mapping.at(std::make_pair(root, RHS_NODE_TYPE)));
  }
  return *(mapping.at(std::make_pair(root, LHS_NODE_TYPE)));
}

binary_leaf::binary_leaf(mapped_object::node_info info) : info_(info){ }

void binary_leaf::process_recursive(kernel_generation_stream & stream, leaf_t leaf, std::map<std::string, std::string> const & accessors, std::set<std::string> & already_fetched)
{
  process(stream, leaf, accessors, *info_.math_expression, info_.root_idx, *info_.mapping, already_fetched);
}

std::string binary_leaf::evaluate_recursive(leaf_t leaf, std::map<std::string, std::string> const & accessors)
{
  return evaluate(leaf, accessors, *info_.math_expression, info_.root_idx, *info_.mapping);
}


mapped_gemm::mapped_gemm(std::string const & scalartype, unsigned int id, node_info info) : mapped_object(scalartype, id, "gemm"), binary_leaf(info) { }

//
mapped_dot::mapped_dot(std::string const & scalartype, unsigned int id, node_info info, std::string const & type_key) :
  mapped_object(scalartype, id, type_key), binary_leaf(info)
{ }

size_t mapped_dot::root_idx() const
{ return info_.root_idx; }

isaac::math_expression const & mapped_dot::math_expression() const
{ return *info_.math_expression; }

math_expression::node mapped_dot::root_node() const
{ return math_expression().tree()[root_idx()]; }

bool mapped_dot::is_index_dot() const
{
  op_element const & op = root_op();
  return op.type==OPERATOR_ELEMENT_ARGFMAX_TYPE
      || op.type==OPERATOR_ELEMENT_ARGMAX_TYPE
      || op.type==OPERATOR_ELEMENT_ARGFMIN_TYPE
      || op.type==OPERATOR_ELEMENT_ARGMIN_TYPE;
}

op_element mapped_dot::root_op() const
{
    return info_.math_expression->tree()[info_.root_idx].op;
}


//
mapped_scalar_dot::mapped_scalar_dot(std::string const & scalartype, unsigned int id, node_info info) : mapped_dot(scalartype, id, info, "scalar_dot"){ }

//
mapped_gemv::mapped_gemv(std::string const & scalartype, unsigned int id, node_info info) : mapped_dot(scalartype, id, info, "gemv") { }

//
void mapped_host_scalar::preprocess(std::string & str) const
{
  struct Morph : public MorphBase
  {
    std::string operator()(std::string const &) const
    { return "#name"; }

    std::string operator()(std::string const &, std::string const &) const
    { return "#name"; }
  };
  replace_macro(str, "$VALUE", Morph());
}

//
mapped_placeholder::mapped_placeholder(unsigned int level) : mapped_object("int", "sforidx" + tools::to_string(level), "placeholder"){}

//
mapped_host_scalar::mapped_host_scalar(std::string const & scalartype, unsigned int id) : mapped_object(scalartype, id, "host_scalar"){ }

//
mapped_handle::mapped_handle(std::string const & scalartype, unsigned int id, std::string const & type_key) : mapped_object(scalartype, id, type_key)
{ register_attribute(pointer_, "#pointer", name_ + "_pointer"); }

//
mapped_buffer::mapped_buffer(std::string const & scalartype, unsigned int id, std::string const & type_key) : mapped_handle(scalartype, id, type_key){ }

//
void mapped_array::preprocess(std::string & str) const
{

  struct MorphValue : public MorphBase
  {
    MorphValue(std::string const & _ld) : ld(_ld){ }

    std::string operator()(std::string const & i) const
    { return "#pointer[" + i + "]"; }

    std::string operator()(std::string const & i, std::string const & j) const
    { return "#pointer[(" + i + ") +  (" + j + ") * " + ld + "]"; }
  private:
    std::string const & ld;
  };

  replace_macro(str, "$VALUE", MorphValue(ld_));
}

mapped_array::mapped_array(std::string const & scalartype, unsigned int id, std::string const & type) : mapped_buffer(scalartype, id, type), effdim_(std::count(type.begin(), type.end(), 'n'))
{
  register_attribute(start_, "#start", name_ + "_start");
  if(effdim_ > 0)
    register_attribute(stride_, "#stride", name_ + "_stride");
  if(effdim_ > 1)
    register_attribute(ld_, "#ld", name_ + "_ld");
}

//
void mapped_vdiag::postprocess(std::string &res) const
{
  std::map<std::string, std::string> accessors;
  tools::find_and_replace(res, "#diag_offset", isaac::evaluate(RHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping));
  accessors["arrayn"] = res;
  accessors["host_scalar"] = res;
  res = isaac::evaluate(LHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping);
}

mapped_vdiag::mapped_vdiag(std::string const & scalartype, unsigned int id, node_info info) : mapped_object(scalartype, id, "vdiag"), binary_leaf(info){}

//
void mapped_array_access::postprocess(std::string &res) const
{
  std::map<std::string, std::string> accessors;
  tools::find_and_replace(res, "#index", isaac::evaluate(RHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping));
  accessors["arrayn"] = res;
  accessors["arraynn"] = res;
  res = isaac::evaluate(LHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping);
}

mapped_array_access::mapped_array_access(std::string const & scalartype, unsigned int id, node_info info) : mapped_object(scalartype, id, "array_access"), binary_leaf(info)
{ }

//
void mapped_matrix_row::postprocess(std::string &res) const
{
  std::map<std::string, std::string> accessors;
  tools::find_and_replace(res, "#row", isaac::evaluate(RHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping));
  accessors["arraynn"] = res;
  res = isaac::evaluate(LHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping);
}

mapped_matrix_row::mapped_matrix_row(std::string const & scalartype, unsigned int id, node_info info) : mapped_object(scalartype, id, "matrix_row"), binary_leaf(info)
{ }

//
void mapped_matrix_column::postprocess(std::string &res) const
{
  std::map<std::string, std::string> accessors;
  tools::find_and_replace(res, "#column", isaac::evaluate(RHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping));
  accessors["arraynn"] = res;
  res = isaac::evaluate(LHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping);
}

mapped_matrix_column::mapped_matrix_column(std::string const & scalartype, unsigned int id, node_info info) : mapped_object(scalartype, id, "matrix_column"), binary_leaf(info)
{ }




//
char mapped_repeat::get_type(node_info const & info)
{
  math_expression::container_type const & tree = info.math_expression->tree();
  size_t tuple_root = tree[info.root_idx].rhs.node_index;

  int sub0 = tuple_get(tree, tuple_root, 2);
  int sub1 = tuple_get(tree, tuple_root, 3);
  if(sub0>1 && sub1==1)
    return 'c';
  else if(sub0==1 && sub1>1)
    return 'r';
  else
    return 'm';
}



void mapped_repeat::postprocess(std::string &res) const
{
  std::map<std::string, std::string> accessors;
  math_expression::container_type const & tree = info_.math_expression->tree();
  size_t tuple_root = tree[info_.root_idx].rhs.node_index;

  tools::find_and_replace(res, "#rep0", get(tree, tuple_root, *info_.mapping, 0).process("#name"));
  tools::find_and_replace(res, "#rep1", get(tree, tuple_root, *info_.mapping, 1).process("#name"));
  tools::find_and_replace(res, "#sub0", get(tree, tuple_root, *info_.mapping, 2).process("#name"));
  tools::find_and_replace(res, "#sub1", get(tree, tuple_root, *info_.mapping, 3).process("#name"));

  struct MorphValue : public MorphBase
  {
    MorphValue(char _type): type(_type){ }

    std::string operator()(std::string const &) const { return "";}

    std::string operator()(std::string const & i, std::string const & j) const
    {
      if(type=='c') return "$VALUE{" + i + "}";
      else if(type=='r') return "$VALUE{" + j + "}";
      else return "$VALUE{" + i + "," + j + "}";
    }
  private:
    char type;
  };

  replace_macro(res, "$VALUE", MorphValue(type_));
  accessors["arrayn"] = res;
  accessors["arraynn"] = res;
  res = isaac::evaluate(LHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping);
}

mapped_repeat::mapped_repeat(std::string const & scalartype, unsigned int id, node_info info) : mapped_object(scalartype, id, "repeat"), binary_leaf(info), type_(get_type(info))
{
}


//
void mapped_matrix_diag::postprocess(std::string &res) const
{
  std::map<std::string, std::string> accessors;
  tools::find_and_replace(res, "#diag_offset", isaac::evaluate(RHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping));
  accessors["arraynn"] = res;
  res = isaac::evaluate(LHS_NODE_TYPE, accessors, *info_.math_expression, info_.root_idx, *info_.mapping);
}

mapped_matrix_diag::mapped_matrix_diag(std::string const & scalartype, unsigned int id, node_info info) : mapped_object(scalartype, id, "matrix_diag"), binary_leaf(info)
{ }

//
void mapped_outer::postprocess(std::string &res) const
{
    struct Morph : public MorphBase
    {
      Morph(leaf_t const & leaf, node_info const & i) : leaf_(leaf), i_(i){}
      std::string operator()(std::string const & i) const
      {
        std::map<std::string, std::string> accessors;
        accessors["arrayn"] = "$VALUE{"+i+"}";
        accessors["array1"] = "#namereg";
        return isaac::evaluate(leaf_, accessors, *i_.math_expression, i_.root_idx, *i_.mapping);
      }
      std::string operator()(std::string const &, std::string const &) const{return "";}
    private:
      leaf_t leaf_;
      node_info i_;
    };

    replace_macro(res, "$LVALUE", Morph(LHS_NODE_TYPE, info_));
    replace_macro(res, "$RVALUE", Morph(RHS_NODE_TYPE, info_));
}

mapped_outer::mapped_outer(std::string const & scalartype, unsigned int id, node_info info) : mapped_object(scalartype, id, "outer"), binary_leaf(info)
{ }

std::string mapped_cast::operator_to_str(operation_node_type type)
{
  switch(type)
  {
    case OPERATOR_CAST_BOOL_TYPE : return "bool";
    case OPERATOR_CAST_CHAR_TYPE : return "char";
    case OPERATOR_CAST_UCHAR_TYPE : return "uchar";
    case OPERATOR_CAST_SHORT_TYPE : return "short";
    case OPERATOR_CAST_USHORT_TYPE : return "ushort";
    case OPERATOR_CAST_INT_TYPE : return "int";
    case OPERATOR_CAST_UINT_TYPE : return "uint";
    case OPERATOR_CAST_LONG_TYPE : return "long";
    case OPERATOR_CAST_ULONG_TYPE : return "ulong";
    case OPERATOR_CAST_HALF_TYPE : return "half";
    case OPERATOR_CAST_FLOAT_TYPE : return "float";
    case OPERATOR_CAST_DOUBLE_TYPE : return "double";
    default : return "invalid";
  }
}

mapped_cast::mapped_cast(operation_node_type type, unsigned int id) : mapped_object(operator_to_str(type), id, "cast")
{ }


}
