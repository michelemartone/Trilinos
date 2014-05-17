// @HEADER
// ***********************************************************************
//
//          Tpetra: Templated Linear Algebra Services Package
//                 Copyright (2008) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
// @HEADER

#ifndef TPETRA_EXPERIMENTAL_BLOCKCRSMATRIX_DECL_HPP
#define TPETRA_EXPERIMENTAL_BLOCKCRSMATRIX_DECL_HPP

/// \file Tpetra_Experimental_BlockCrsMatrix_decl.hpp
/// \brief Declaration of Tpetra::Experimental::BlockCrsMatrix

#include <Tpetra_CrsGraph.hpp>
#include <Tpetra_Operator.hpp>
#include <Tpetra_Experimental_BlockMultiVector.hpp>

namespace Tpetra {
namespace Experimental {

/// \class BlockCrsMatrix
/// \author Mark Hoemmen
/// \date 13 Feb 2014, 24 Feb 2014
///
/// Please read the documentation of BlockMultiVector first.
///
/// This class stores values associated with the degrees of freedom of
/// a single mesh point contiguously, in a getBlockSize() by
/// getBlockSize() block, in row-major format.
///
/// Since this class requires a fill-complete Tpetra::CrsGraph for
/// construction, it has a row and column Map already.  This means
/// that it only needs to provide access using local indices.  Users
/// are responsible for converting from global to local indices if
/// necessary.  Please be aware that the row Map and column Map may
/// differ, so you may not use local row and column indices
/// interchangeably.
///
/// For simplicity, this object only supports local indexing.  It can
/// do so because both of its constructors require a fill-complete
/// Tpetra::CrsGraph, which therefore has both a row Map and a column
/// Map.
///
/// Here is an example of how to fill into this object using direct
/// views.
///
/// \code
/// int err = 0;
/// // At least one entry, so &offsets[0] always makes sense.
/// std::vector<ptrdiff_t> offsets (1);
/// for (LO localRowInd = 0; localRowInd < localNumRows; ++localRowInd) {
///   const LO numEntries = A.getNumEntriesInLocalRow (localRowInd);
///
///   if (wantOffsets) {
///     // It's not necessary to get offsets if you plan to change
///     // _all_ the entries in the row.  If you only plan to change
///     // a subset of entries in the row, you should forego calling
///     // getLocalRowView(), and instead call
///     // replaceLocalValuesByOffsets().
///     if (offsets.size () < numEntries) {
///       offsets.resize (numEntries);
///     }
///     err = A.getLocalRowOffsets (localRowInd, &offsets[0]);
///     if (err != 0) {
///       break;
///     }
///   }
///
///   // Get a view of the current row.
///   // You may modify the values, but not the column indices.
///   const LO* localColInds;
///   Scalar* vals;
///   err = A.getLocalRowView (localRowInd, localColInds, vals, numEntries);
///   if (err != 0) {
///     break;
///   }
///
///   // Modify the entries in the current row.
///   for (LO k = 0; k < numEntries; ++k) {
///     Scalar* const curBlock = vals[blockSize * blockSize * k];
///     // Blocks are stored in row-major format.
///     for (LO j = 0; j < blockSize; ++j) {
///       for (LO i = 0; i < blockSize; ++i) {
///         const Scalar curVal = curBlock[i + j * blockSize];
///         // Some function f of the current value and mesh point
///         curBlock[i + j * blockSize] = f (curVal, localColInds[k], ...);
///       }
///     }
///   }
/// }
/// \endcode
///
template<class Scalar, class LO = int,  class GO = LO,
         class Node = KokkosClassic::DefaultNode::DefaultNodeType>
class BlockCrsMatrix :
  public Tpetra::Operator<Scalar, LO, GO, Node>,
  Tpetra::DistObject<char, LO, GO, Node>
{
private:
  typedef Tpetra::DistObject<char, LO, GO, Node> dist_object_type;
  typedef BlockMultiVector<Scalar, LO, GO, Node> BMV;
  typedef Teuchos::ScalarTraits<Scalar> STS;

protected:
  typedef char packet_type;

public:
  //! \name Public typedefs
  //@{

  //! The type of entries in the matrix.
  typedef Scalar scalar_type;
  //! The type of local indices.
  typedef LO local_ordinal_type;
  //! The type of global indices.
  typedef GO global_ordinal_type;
  //! The Kokkos Node type.
  typedef Node node_type;

  typedef ::Tpetra::Map<LO, GO, node_type> map_type;
  typedef Tpetra::MultiVector<scalar_type, LO, GO, node_type> mv_type;
  typedef Tpetra::CrsGraph<LO, GO, node_type> crs_graph_type;

  typedef LittleBlock<Scalar, LO> little_block_type;
  typedef LittleBlock<const Scalar, LO> const_little_block_type;
  typedef LittleVector<Scalar, LO> little_vec_type;
  typedef LittleVector<const Scalar, LO> const_little_vec_type;

  //@}
  //! \name Constructors and destructor
  //@{

  //! Default constructor: Makes an empty block matrix.
  BlockCrsMatrix ();

  /// \brief Constructor that takes a graph and a block size.
  ///
  /// The graph represents the mesh.  This constructor computes the
  /// point Maps corresponding to the given graph's domain and range
  /// Maps.  If you already have those point Maps, it is better to
  /// call the four-argument constructor.
  ///
  /// \param graph [in] A fill-complete graph.
  /// \param blockSize [in] Number of degrees of freedom per mesh point.
  BlockCrsMatrix (const crs_graph_type& graph, const LO blockSize);

  /// \brief Constructor that takes a graph, domain and range point
  ///   Maps, and a block size.
  ///
  /// The graph represents the mesh.  This constructor uses the given
  /// domain and range point Maps, rather than computing them.  The
  /// given point Maps must be the same as the above two-argument
  /// constructor would have computed.
  BlockCrsMatrix (const crs_graph_type& graph,
                  const map_type& domainPointMap,
                  const map_type& rangePointMap,
                  const LO blockSize);

  //! Destructor (declared virtual for memory safety).
  virtual ~BlockCrsMatrix () {}

  //@}
  //! \name Implementation of Tpetra::Operator
  //@{

  //! Get the (point) domain Map of this matrix.
  Teuchos::RCP<const map_type> getDomainMap () const;

  //! Get the (point) range Map of this matrix.
  Teuchos::RCP<const map_type> getRangeMap () const;

  /// \brief For this matrix A, compute <tt>Y := beta * Y + alpha * Op(A) * X</tt>.
  ///
  /// Op(A) is A if mode is Teuchos::NO_TRANS, the transpose of A if
  /// mode is Teuchos::TRANS, and the conjugate transpose of A if mode
  /// is Teuchos::CONJ_TRANS.
  ///
  /// If alpha is zero, ignore X's entries on input; if beta is zero,
  /// ignore Y's entries on input.  This follows the BLAS convention,
  /// and only matters if X resp. Y have Inf or NaN entries.
  void
  apply (const mv_type& X,
         mv_type& Y,
         Teuchos::ETransp mode = Teuchos::NO_TRANS,
         scalar_type alpha = Teuchos::ScalarTraits<scalar_type>::one (),
         scalar_type beta = Teuchos::ScalarTraits<scalar_type>::zero ()) const;

  /// \brief Whether it is valid to apply the transpose or conjugate
  ///   transpose of this matrix.
  bool hasTransposeApply () const {
    // FIXME (mfh 04 May 2014) Transpose and conjugate transpose modes
    // are not implemented yet.  Fill in applyBlockTrans() to fix this.
    return false;
  }

  //@}
  //! \name Block operations
  //@{

  //! The number of degrees of freedom per mesh point.
  LO getBlockSize () const { return blockSize_; }

  /// \brief Version of apply() that takes BlockMultiVector input and output.
  ///
  /// This method is deliberately not marked const, because it may do
  /// lazy initialization of temporary internal block multivectors.
  void
  applyBlock (const BlockMultiVector<Scalar, LO, GO, Node>& X,
              BlockMultiVector<Scalar, LO, GO, Node>& Y,
              Teuchos::ETransp mode = Teuchos::NO_TRANS,
              const Scalar alpha = Teuchos::ScalarTraits<Scalar>::one (),
              const Scalar beta = Teuchos::ScalarTraits<Scalar>::zero ());

  /// \brief Replace values at the given column indices, in the given row.
  ///
  /// \param localRowInd [in] Local index of the row in which to replace.
  ///
  /// \param colInds [in] Local column ind{ex,ices} at which to
  ///   replace values.  colInds[k] is the local column index whose
  ///   new values start at vals[getBlockSize() * getBlockSize() * k],
  ///   and colInds has length at least numColInds.  This method will
  ///   only access the first numColInds entries of colInds.
  ///
  /// \param vals [in] The new values to use at the given column
  ///   indices.
  ///
  /// \param numColInds [in] The number of entries of colInds.
  ///
  /// \return The number of valid column indices in colInds.  This
  ///   method succeeded if and only if the return value equals the
  ///   input argument numColInds.
  LO
  replaceLocalValues (const LO localRowInd,
                      const LO colInds[],
                      const Scalar vals[],
                      const LO numColInds) const;

  /// \brief Sum into values at the given column indices, in the given row.
  ///
  /// \param localRowInd [in] Local index of the row into which to sum.
  ///
  /// \param colInds [in] Local column ind{ex,ices} at which to sum
  ///   into values.  colInds[k] is the local column index whose new
  ///   values start at vals[getBlockSize() * getBlockSize() * k], and
  ///   colInds has length at least numColInds.  This method will only
  ///   access the first numColInds entries of colInds.
  ///
  /// \param vals [in] The new values to sum into at the given column
  ///   indices.
  ///
  /// \param numColInds [in] The number of entries of colInds.
  ///
  /// \return Zero if the sum was successful, else nonzero.
  LO
  sumIntoLocalValues (const LO localRowInd,
                      const LO colInds[],
                      const Scalar vals[],
                      const LO numColInds) const;

  /// \brief Get a view of the row, using local indices.
  ///
  /// Since this object has a graph (which we assume is fill complete
  /// on input to the constructor), it has a column Map, and it stores
  /// column indices as local indices.  This means you can view the
  /// column indices as local indices directly.  However, you may
  /// <i>not</i> view them as global indices directly, since the
  /// column indices are not stored that way in the graph.
  ///
  /// \return Zero if successful (if the local row is valid on the
  ///   calling process), else nonzero.
  LO
  getLocalRowView (const LO localRowInd,
                   const LO*& colInds,
                   Scalar*& vals,
                   LO& numInds) const;

  /// \brief Get offsets corresponding to the given row indices.
  ///
  /// The point of this method is to precompute the results of
  /// searching for the offsets corresponding to the given column
  /// indices.  You may then reuse these search results in
  /// replaceLocalValuesByOffsets or sumIntoLocalValuesByOffsets.
  ///
  /// Offsets are for column indices, <i>not</i> for values.
  /// That is, the blockSize stride is precomputed.
  ///
  /// \param localRowInd [in] Index of the local row.
  /// \param offsets [out] On output: offsets corresponding to the
  ///   given column indices.  Must have at least numColInds entries.
  /// \param colInds [in] The local column indices for which to
  ///   compute entries.  Must have at least numColInds entries.
  ///   This method will only read the first numColsInds entries.
  /// \param numColInds [in] Number of entries in colInds to read.
  LO
  getLocalRowOffsets (const LO localRowInd,
                      ptrdiff_t offsets[],
                      const LO colInds[],
                      const LO numColInds) const;

  //! Like the four-argument version, but for all entries in the row.
  LO
  getLocalRowOffsets (const LO localRowInd,
                      ptrdiff_t offsets[]) const;

  //! Like replaceLocalValues, but avoids computing row offsets via search.
  LO
  replaceLocalValuesByOffsets (const LO localRowInd,
                               const ptrdiff_t offsets[],
                               const Scalar vals[],
                               const LO numOffsets) const;

  //! Like sumIntoLocalValues, but avoids computing row offsets via search.
  LO
  sumIntoLocalValuesByOffsets (const LO localRowInd,
                               const ptrdiff_t offsets[],
                               const Scalar vals[],
                               const LO numOffsets) const;

  /// \brief Return the number of entries in the given row on the
  ///   calling process.
  ///
  /// If the given local row index is invalid, this method (sensibly)
  /// returns zero, since the calling process trivially does not own
  /// any entries in that row.
  LO getNumEntriesInLocalRow (const LO localRowInd) const;

protected:
  /// \brief \name Implementation of DistObject (or DistObjectKA).
  ///
  /// The methods here implement Tpetra::DistObject or
  /// Tpetra::DistObjectKA, depending on a configure-time option.
  /// They let BlockMultiVector participate in Import and Export
  /// operations.  Users don't have to worry about these methods.
  //@{

  virtual bool checkSizes (const Tpetra::SrcDistObject& /* source */ ) {
    return false; // not implemented
  }

  virtual void
  copyAndPermute (const Tpetra::SrcDistObject& source,
                  size_t numSameIDs,
                  const Teuchos::ArrayView<const LO>& permuteToLIDs,
                  const Teuchos::ArrayView<const LO>& permuteFromLIDs)
  {
    (void) source;
    (void) numSameIDs;
    (void) permuteToLIDs;
    (void) permuteFromLIDs;
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Not implemented");
  }

  virtual void
  packAndPrepare (const Tpetra::SrcDistObject& source,
                  const Teuchos::ArrayView<const LO>& exportLIDs,
                  Teuchos::Array<packet_type>& exports,
                  const Teuchos::ArrayView<size_t>& numPacketsPerLID,
                  size_t& constantNumPackets,
                  Tpetra::Distributor& distor)
  {
    (void) source;
    (void) exportLIDs;
    (void) exports;
    (void) numPacketsPerLID;
    (void) constantNumPackets;
    (void) distor;
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Not implemented");
  }

  virtual void
  unpackAndCombine (const Teuchos::ArrayView<const LO> &importLIDs,
                    const Teuchos::ArrayView<const packet_type> &imports,
                    const Teuchos::ArrayView<size_t> &numPacketsPerLID,
                    size_t constantNumPackets,
                    Tpetra::Distributor& distor,
                    Tpetra::CombineMode CM)
  {
    (void) importLIDs;
    (void) imports;
    (void) numPacketsPerLID;
    (void) constantNumPackets;
    (void) distor;
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Not implemented");
  }
  //@}

private:
  //! The graph that describes the structure of this matrix.
  crs_graph_type graph_;
  /// \brief The point Map version of the graph's domain Map.
  ///
  /// NOTE (mfh 16 May 2014) Since this is created at construction
  /// time, we don't have to worry about the lazy initialization
  /// issue, that we <i>do</i> have to worry about for X_colMap_ and
  /// Y_rowMap_.
  map_type domainPointMap_;
  /// \brief The point Map version of the graph's range Map.
  ///
  /// NOTE (mfh 16 May 2014) Since this is created at construction
  /// time, we don't have to worry about the lazy initialization
  /// issue, that we <i>do</i> have to worry about for X_colMap_ and
  /// Y_rowMap_.
  map_type rangePointMap_;
  //! The number of degrees of freedom per mesh point.
  LO blockSize_;
  //! Raw pointer to the graph's array of row offsets.
  const size_t* ptr_;
  //! Raw pointer to the graph's array of column indices.
  const LO* ind_;
  //! Array of values in the matrix.
  Teuchos::Array<Scalar> val_;

  /// \brief Column Map block multivector (only initialized if needed).
  ///
  /// FIXME (mfh 16 May 2014) Use a pointer (RCP or std::unique_ptr)
  /// here, for correct use of the "lazy initialization of members of
  /// objects with view semantics" pattern.  (Otherwise, existing
  /// views of the BlockCrsMatrix won't get the benefit of lazy
  /// initialization.)
  BMV X_colMap_;
  /// \brief Row Map block multivector (only initialized if needed).
  ///
  /// FIXME (mfh 16 May 2014) Use a pointer (RCP or std::unique_ptr)
  /// here, for correct use of the "lazy initialization of members of
  /// objects with view semantics" pattern.  (Otherwise, existing
  /// views of the BlockCrsMatrix won't get the benefit of lazy
  /// initialization.)
  BMV Y_rowMap_;
  //! Whether the column Map block multivector is initialized.
  bool X_colMap_initialized_;
  //! Whether the row Map block multivector is initialized.
  bool Y_rowMap_initialized_;

  /// \brief Padding to use for "little blocks" in the matrix.
  ///
  /// If this is nonzero, we pad the number of columns in each little
  /// block up to a multiple of the padding value.  This will let us
  /// potentially do explicit short-vector SIMD in the dense little
  /// block matrix-vector multiply.  We got this idea from Kendall
  /// Pierson's FETI code (thanks Kendall!).
  LO columnPadding_;

  //! Whether "little blocks" are stored in row-major (or column-major) order.
  bool rowMajor_;

  /// \brief Global sparse matrix-vector multiply for the transpose or
  ///   conjugate transpose cases.
  ///
  /// This method computes Y := beta*Y + alpha*Op(A)*X, where A is
  /// *this (the block matrix), Op(A) signifies either the transpose
  /// or the conjugate transpose of A, and X and Y are block
  /// multivectors.  The special cases alpha = 0 resp. beta = 0 have
  /// their usual BLAS meaning; this only matters if (A or X) resp. Y
  /// contain Inf or NaN values.
  void
  applyBlockTrans (const BlockMultiVector<Scalar, LO, GO, Node>& X,
                   BlockMultiVector<Scalar, LO, GO, Node>& Y,
                   const Teuchos::ETransp mode,
                   const Scalar alpha,
                   const Scalar beta);

  /// \brief Global sparse matrix-vector multiply for the non-transpose case.
  ///
  /// This method computes Y := beta*Y + alpha*A*X, where A is *this
  /// (the block matrix), and X and Y are block multivectors.  The
  /// special cases alpha = 0 resp. beta = 0 have their usual BLAS
  /// meaning; this only matters if (A or X) resp. Y contain Inf or
  /// NaN values.
  void
  applyBlockNoTrans (const BlockMultiVector<Scalar, LO, GO, Node>& X,
                     BlockMultiVector<Scalar, LO, GO, Node>& Y,
                     const Scalar alpha,
                     const Scalar beta);

  /// \brief Local sparse matrix-vector multiply for the non-transpose case.
  ///
  /// This method computes Y := beta*Y + alpha*A*X, where A is *this
  /// (the block matrix), and X and Y are block multivectors.  The
  /// special cases alpha = 0 resp. beta = 0 have their usual BLAS
  /// meaning; this only matters if (A or X) resp. Y contain Inf or
  /// NaN values.
  void
  localApplyBlockNoTrans (const BlockMultiVector<Scalar, LO, GO, Node>& X,
                          BlockMultiVector<Scalar, LO, GO, Node>& Y,
                          const Scalar alpha,
                          const Scalar beta);

  size_t
  findOffsetOfColumnIndex (const LO localRowIndex,
                           const LO colIndexToFind,
                           const size_t hint) const;

  LO allocationSizePerBlock () const;

  const_little_block_type
  getConstLocalBlockFromInput (const Scalar* val, const size_t pointOffset) const;

  little_block_type
  getNonConstLocalBlockFromInput (Scalar* val, const size_t pointOffset) const;

  const_little_block_type
  getConstLocalBlockFromOffset (const size_t blockOffset) const;

  little_block_type
  getNonConstLocalBlockFromOffset (const size_t blockOffset) const;
};

} // namespace Experimental
} // namespace Tpetra

#endif // TPETRA_EXPERIMENTAL_BLOCKCRSMATRIX_DECL_HPP