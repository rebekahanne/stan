#ifndef __STAN__IO__WRITER_HPP__
#define __STAN__IO__WRITER_HPP__

#include <cassert>
#include <vector>
#include <Eigen/Dense>
#include <boost/multi_array.hpp>
#include <stan/maths/special_functions.hpp>

namespace stan {

  namespace io {

    using Eigen::Array;
    using Eigen::Dynamic;
    using Eigen::LDLT;
    using Eigen::Matrix;
    using Eigen::DiagonalMatrix;

    /**
     * A stream-based writer for integer, scalar, vector, matrix
     * and array data types, which transforms from constrained to
     * a sequence of constrained variables.  
     *
     * <p>This class converts constrained values to unconstrained
     * values with mappings that invert those defined in
     * <code>stan::io::reader</code> to convert unconstrained values
     * to constrained values.
     *
     * @tparam T Basic scalar type.
     */
    template <typename T>
    class writer {
    private:
      std::vector<T> data_r_;
      std::vector<int> data_i_;
    public:


      /**
       * This is the tolerance for checking arithmetic bounds
       * in rank and in simplexes.  The current value is <code>1E-8</code>.
       */
      const double CONSTRAINT_TOLERANCE;

      /**
       * Construct a writer that writes to the specified
       * scalar and integer vectors.
       *
       * @param data_r Scalar values.
       * @param data_i Integer values.
       */
      writer(std::vector<T>& data_r,
	     std::vector<int>& data_i)
	: data_r_(data_r),
	  data_i_(data_i),
	  CONSTRAINT_TOLERANCE(1E-8) {
      }

      /**
       * Destroy this writer.
       */
      ~writer() { }

      /**
       * Return a reference to the underlying vector of real values
       * that have been written.
       *
       * @return Values that have been written.
       */
      std::vector<T>& data_r() {
	return &data_r_;
      }


      /**
       * Return a reference to the underlying vector of integer values
       * that have been written.
       *
       * @return Values that have been written.
       */
      std::vector<int>& data_i() {
	return &data_i_;
      }

      /**
       * Write the unconstrained value corresponding to the specified
       * scalar.  Here, the unconstrain operation is a no-op, which
       * matches <code>reader::scalar_constrain()</code>.
       *
       * @param x The value.
       */
      void scalar_unconstrain(T& y) {
	data_r_.push_back(y);
      }

      /**
       * Write the unconstrained value corresponding to the specified
       * positive-constrained scalar.  The transformation applied is
       * <code>log(y)</code>, which is the inverse of constraining
       * transform specified in <code>reader::scalar_pos_constrain()</code>.
       *
       * <p>This method will fail if the argument is not non-negative.
       *
       * @param y The positive value.
       */
      void scalar_pos_unconstrain(T& y) {
	assert(y >= 0.0);
	data_r_.push_back(log(y));
      }

      /**
       * Return the unconstrained version of the specified input,
       * which is constrained to be above the specified lower bound.
       * The unconstraining transform is <code>log(y - lb)</code>, which 
       * inverts the constraining
       * transform defined in <code>reader::scalar_lb_constrain(double)</code>,
       *
       * @param lb Lower bound.
       * @param y Lower-bounded value.
       */
      void scalar_lb_unconstrain(double lb, T& y) {
	assert(y >= lb);
	data_r_.push_back(log(y - lb));
      }

      /**
       * Write the unconstrained value corresponding to the specified
       * lower-bounded value.  The unconstraining transform is
       * <code>log(ub - y)</code>, which reverses the constraining
       * transform defined in <code>reader::scalar_ub_constrain(double)</code>.
       *
       * @param ub Upper bound.
       * @param y Constrained value.
       */
      void scalar_ub_unconstrain(double ub, T& y) {
	assert(y <= ub);
	data_r_.push_back(log(ub - y));
      }

      /**
       * Write the unconstrained value corresponding to the specified
       * value with the specified bounds.  The unconstraining
       * transform is given by <code>reader::logit((y-L)/(U-L))</code>, which
       * inverts the constraining transform defined in
       * <code>scalar_lub_constrain(double,double)</code>.
       *
       * @param lb Lower bound.
       * @param ub Upper bound.
       * @param y Bounded value.
       */
      void scalar_lub_unconstrain(double lb, double ub, T& y) {
	assert(lb <= y);
	assert(y <= ub);
	data_r_.push_back(logit((y - lb) / (ub - lb)));
      }

      /**
       * Write the unconstrained value corresponding to the specified
       * correlation-constrained variable.
       *
       * <p>The unconstraining transform is <code>atanh(y)</code>, which
       * reverses the transfrom in <code>corr_constrain()</code>.
       *
       * @param y Correlation value.
       */
      void corr_unconstrain(T& y) {
	assert(-1.0 <= y);
	assert(y <= 1.0);
	data_r_.push_back(atanh(y));
      }

      /**
       * Write the unconstrained value corresponding to the
       * specified probability value.
       *
       * <p>The unconstraining transform is <code>logit(y)</code>,
       * which inverts the constraining transform defined in
       * <code>prob_constrain()</code>.
       *
       * @param y Probability value.
       */
      void prob_unconstrain(T& y) {
	assert(0.0 <= y);
	assert(y <= 1.0);
	data_r_.push_back(logit(y));
      }

      /**
       * Write the unconstrained vector that corresponds to the specified
       * positive ordered vector.
       * 
       * <p>The unconstraining transform is defined for input vector <code>y</code>
       * to produce an output vector <code>x</code> of the same size, defined
       * by <code>x[0] = log(y[0])</code> and by
       * <code>x[k] = log(y[k] - y[k-1])</code> for <code>k > 0</code>.  This
       * unconstraining transform inverts the constraining transform specified
       * in <code>pos_ordered_constrain(unsigned int)</code>.
       *
       * @param y Positive, ordered vector.
       * @return Unconstrained vector corresponding to the specified vector.
       */
      void pos_ordered_unconstrain(Matrix<T,Dynamic,1>& y) {
	if (y.size() == 0) return;
	assert(y[0] >= 0.0);
	data_r_.push_back(log(y[0]));
	for (unsigned int i = 1; i < y.size(); ++i) {
	  assert(y[i] >= y[i-1]);
	  data_r_.push_back(log(y[i] - y[i-1]));
	}
      }


      /**
       * Return the unconstrained vector corresponding to the specified simplex 
       * value.  If the specified constrained simplex is of size <code>K</code>,
       * the returned unconstrained vector is of size <code>K-1</code>.
       *
       * <p>The transform takes <code>y = y[1],...,y[K]</code> and
       * produces the unconstrained vector <code>x = log(y[1]) -
       * log(y[K]), ..., log(y[K-1]) - log(y[K])</code>.  This inverts
       * the constraining transform of
       * <code>simplex_constrain(unsigned int)</code>.
       *
       * @param y Simplex constrained value.
       * @return Unconstrained value.
       */
      void simplex_unconstrain(Matrix<T,Dynamic,1>& y) {
	assert(y.size() > 0);
	assert(abs(1.0 - y.sum()) < CONSTRAINT_TOLERANCE);
	unsigned int k_minus_1 = y.size() - 1;
	double log_y_k = log(y[k_minus_1]);
	for (unsigned int i = 0; i < k_minus_1; ++i) {
	  assert(y[i] >= 0.0);
	  data_r_.push_back(log(y[i]) - log_y_k);
	}
      }

      /**
       * Writes the unconstrained correlation matrix corresponding
       * to the specified constrained correlation matrix.
       *
       * <p>The unconstraining operation is the inverse of the
       * constraining operation in
       * <code>corr_matrix_constrain(Matrix<T,Dynamic,Dynamic)</code>.
       *
       * @param y Constrained correlation matrix.
       */
      void corr_matrix_unconstrain(Matrix<T,Dynamic,Dynamic>& y) {
	unsigned int k = y.rows();
	assert(k > 0);
	assert(y.cols() == k);
	unsigned int k_choose_2 = (k * (k-1)) / 2;
	Array<T,Dynamic,1> cpcs(k_choose_2);
	Array<T,Dynamic,1> sds(k);
	bool successful = factor_cov_matrix(cpcs,sds,y);
	for (unsigned int i = 0; i < k; ++i)
	  assert(abs(sds[i] - 1.0) < CONSTRAINT_TOLERANCE);
	assert(successful);
	for (unsigned int i = 0; i < k_choose_2; ++i)
	  data_r_.push_back(cpcs[i]);
      }

      /**
       * Writes the unconstrained covariance matrix corresponding
       * to the specified constrained correlation matrix.
       *
       * <p>The unconstraining operation is the inverse of the
       * constraining operation in
       * <code>cov_matrix_constrain(Matrix<T,Dynamic,Dynamic)</code>.
       *
       * @param y Constrained covariance matrix.
       */
      void cov_matrix_unconstrain(Matrix<T,Dynamic,Dynamic>& y) {
	unsigned int k = y.rows();
	assert(k > 0);
	assert(y.cols() == k);
	unsigned int k_choose_2 = (k * (k-1)) / 2;
	Array<T,Dynamic,1> cpcs(k_choose_2);
	Array<T,Dynamic,1> sds(k);
	bool successful = factor_cov_matrix(cpcs,sds,y);
	assert(successful);
	for (unsigned int i = 0; i < k_choose_2; ++i)
	  data_r_.push_back(cpcs[i]);
	for (unsigned int i = 0; i < k; ++i)
	  data_r_.push_back(sds[i]);
      }

      
    };


  }

}

#endif
