#ifndef ISAAC_MAPPED_OBJECT_H
#define ISAAC_MAPPED_OBJECT_H

#include <map>
#include <string>
#include "isaac/types.h"
#include "isaac/kernels/stream.h"
#include "isaac/symbolic/expression.h"

namespace isaac
{

enum leaf_t
{
  LHS_NODE_TYPE,
  PARENT_NODE_TYPE,
  RHS_NODE_TYPE
};

class mapped_object;

typedef std::pair<size_t, leaf_t> mapping_key;
typedef std::map<mapping_key, std::shared_ptr<mapped_object> > mapping_type;




/** @brief Mapped Object
*
* This object populates the symbolic mapping associated with a math_expression. (root_id, LHS|RHS|PARENT) => mapped_object
* The tree can then be reconstructed in its symbolic form
*/
class mapped_object
{
private:
  virtual void preprocess(std::string &) const;
  virtual void postprocess(std::string &) const;

protected:
  struct MorphBase {
      virtual std::string operator()(std::string const & i) const = 0;
      virtual std::string operator()(std::string const & i, std::string const & j) const = 0;
      virtual ~MorphBase(){}
  };

  static void replace_macro(std::string & str, std::string const &, MorphBase const & morph);
  void register_attribute(std::string & attribute, std::string const & key, std::string const & value);

public:
  struct node_info
  {
    node_info(mapping_type const * _mapping, math_expression const * _math_expression, size_t _root_idx);
    mapping_type const * mapping;
    isaac::math_expression const * math_expression;
    size_t root_idx;
  };

public:
  mapped_object(std::string const & scalartype, std::string const & name, std::string const & type_key);
  mapped_object(std::string const & scalartype, unsigned int id, std::string const & type_key);
  virtual ~mapped_object();

  std::string type_key() const;
  std::string const & name() const;
  std::map<std::string, std::string> const & keywords() const;

  std::string process(std::string const & in) const;
  std::string evaluate(std::map<std::string, std::string> const & accessors) const;
protected:
  std::string type_key_;
  std::string name_;
  std::string scalartype_;
  std::map<std::string, std::string> keywords_;
};


class binary_leaf
{
public:
  binary_leaf(mapped_object::node_info info);

  void process_recursive(kernel_generation_stream & stream, leaf_t leaf, std::map<std::string, std::string> const & accessors, std::set<std::string> & already_fetched);
  std::string evaluate_recursive(leaf_t leaf, std::map<std::string, std::string> const & accessors);
protected:
  mapped_object::node_info info_;
};

/** @brief Matrix product
  *
  * Maps prod(matrix_expression, matrix_expression)
  */
class mapped_gemm : public mapped_object, public binary_leaf
{
public:
  mapped_gemm(std::string const & scalartype, unsigned int id, node_info info);
};

/** @brief Reduction
*
* Base class for mapping a dot
*/
class mapped_dot : public mapped_object, public binary_leaf
{
public:
  mapped_dot(std::string const & scalartype, unsigned int id, node_info info, std::string const & type_key);

  size_t root_idx() const;
  isaac::math_expression const & math_expression() const;
  math_expression::node root_node() const;
  bool is_index_dot() const;
  op_element root_op() const;
};

/** @brief Scalar dot
*
* Maps a scalar dot (max, min, argmax, inner_prod, etc..)
*/
class mapped_scalar_dot : public mapped_dot
{
public:
  mapped_scalar_dot(std::string const & scalartype, unsigned int id, node_info info);
};

/** @brief Vector dot
*
* Maps a row-wise dot (max, min, argmax, matrix-vector product, etc..)
*/
class mapped_gemv : public mapped_dot
{
public:
  mapped_gemv(std::string const & scalartype, unsigned int id, node_info info);
};

/** @brief Host scalar
 *
 * Maps a host scalar (passed by value)
 */
class mapped_host_scalar : public mapped_object
{
  void preprocess(std::string & str) const;
public:
  mapped_host_scalar(std::string const & scalartype, unsigned int id);
};

/** @brief Placeholder
 *
 * Maps a placeholder
 */
class mapped_placeholder : public mapped_object
{
public:
  mapped_placeholder(unsigned int level);
};

/** @brief Handle
*
* Maps an object passed by pointer
*/
class mapped_handle : public mapped_object
{
public:
  mapped_handle(std::string const & scalartype, unsigned int id, std::string const & type_key);
private:
  std::string pointer_;
};

/** @brief Buffered
 *
 * Maps a buffered object (vector, matrix)
 */
class mapped_buffer : public mapped_handle
{
public:
  mapped_buffer(std::string const & scalartype, unsigned int id, std::string const & type_key);
};

class mapped_array : public mapped_buffer
{
private:
  void preprocess(std::string & str) const;
public:
  mapped_array(std::string const & scalartype, unsigned int id, std::string const & type);
private:
  std::string ld_;
  std::string start_;
  std::string stride_;
  unsigned int effdim_;
};

class mapped_vdiag : public mapped_object, public binary_leaf
{
private:
  void postprocess(std::string &res) const;
public:
  mapped_vdiag(std::string const & scalartype, unsigned int id, node_info info);
};

class mapped_array_access: public mapped_object, binary_leaf
{
private:
  void postprocess(std::string &res) const;
public:
  mapped_array_access(std::string const & scalartype, unsigned int id, node_info info);
};

class mapped_matrix_row : public mapped_object, binary_leaf
{
private:
  void postprocess(std::string &res) const;
public:
  mapped_matrix_row(std::string const & scalartype, unsigned int id, node_info info);
};

class mapped_matrix_column : public mapped_object, binary_leaf
{
private:
  void postprocess(std::string &res) const;
public:
  mapped_matrix_column(std::string const & scalartype, unsigned int id, node_info info);
};

class mapped_repeat : public mapped_object, binary_leaf
{
private:
  static char get_type(node_info const & info);
  void postprocess(std::string &res) const;
public:
  mapped_repeat(std::string const & scalartype, unsigned int id, node_info info);
private:
  char type_;
};

class mapped_matrix_diag : public mapped_object, binary_leaf
{
private:
  void postprocess(std::string &res) const;
public:
  mapped_matrix_diag(std::string const & scalartype, unsigned int id, node_info info);
};

class mapped_outer : public mapped_object, binary_leaf
{
private:
  void postprocess(std::string &res) const;
public:
  mapped_outer(std::string const & scalartype, unsigned int id, node_info info);
};

class mapped_cast : public mapped_object
{
  static std::string operator_to_str(operation_node_type type);
public:
  mapped_cast(operation_node_type type, unsigned int id);
};

extern mapped_object& get(math_expression::container_type const &, size_t, mapping_type const &, size_t);

}
#endif
