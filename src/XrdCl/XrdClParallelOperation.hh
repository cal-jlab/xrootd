//------------------------------------------------------------------------------
// Copyright (c) 2011-2017 by European Organization for Nuclear Research (CERN)
// Author: Krzysztof Jamrog <krzysztof.piotr.jamrog@cern.ch>,
//         Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef __XRD_CL_PARALLELOPERATION_HH__
#define __XRD_CL_PARALLELOPERATION_HH__

#include "XrdCl/XrdClOperations.hh"
#include "XrdCl/XrdClOperationHandlers.hh"

#include <atomic>

namespace XrdCl
{
  
  //----------------------------------------------------------------------------
  // Interface for different execution policies:
  // - all      : all operations need to succeed in order for the parallel
  //              operation to be successful
  // - any      : just one of the operations needs to succeed in order for
  //              the parallel operation to be successful
  // - some     : n (user defined) operations need to succeed in order for
  //              the parallel operation to be successful
  // - at least : at least n (user defined) operations need to succeed in
  //              order for the parallel operation to be successful (the
  //              user handler will be called only when all operations are
  //              resolved)
  //
  // @param status : status returned by one of the aggregated operations
  //
  // @return       : true if the status should be passed to the user handler,
  //                 false otherwise.
  //----------------------------------------------------------------------------
  struct PolicyExecutor
  {
    virtual ~PolicyExecutor()
    {
    }

    virtual bool Examine( const XrdCl::XRootDStatus &status ) = 0;
  };

  //----------------------------------------------------------------------------
  //! Parallel operations, allows to execute two or more pipelines in
  //! parallel.
  //!
  //! @arg state : describes current operation configuration state
  //!              (@see Operation)
  //----------------------------------------------------------------------------
  template<bool HasHndl>
  class ParallelOperation: public ConcreteOperation<ParallelOperation, HasHndl, Resp<void>>
  {
      template<bool> friend class ParallelOperation;

    public:

      //------------------------------------------------------------------------
      //! Constructor: copy-move a ParallelOperation in different state
      //------------------------------------------------------------------------
      template<bool from>
      ParallelOperation( ParallelOperation<from> &&obj ) :
          ConcreteOperation<ParallelOperation, HasHndl, Resp<void>>( std::move( obj ) ),
          pipelines( std::move( obj.pipelines ) ),
          policy( std::move( obj.policy ) )
      {
      }

      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @arg   Container : iterable container type
      //!
      //! @param container : iterable container with pipelines
      //------------------------------------------------------------------------
      template<class Container>
      ParallelOperation( Container &&container )
      {
        static_assert( !HasHndl, "Constructor is available only operation without handler");

        pipelines.reserve( container.size() );
        auto begin = std::make_move_iterator( container.begin() );
        auto end   = std::make_move_iterator( container.end() );
        std::copy( begin, end, std::back_inserter( pipelines ) );
        container.clear(); // there's junk inside so we clear it
      }

      //------------------------------------------------------------------------
      //! @return : operation name
      //------------------------------------------------------------------------
      std::string ToString()
      {
        std::ostringstream oss;
        oss << "Parallel(";
        for( size_t i = 0; i < pipelines.size(); i++ )
        {
          oss << pipelines[i]->ToString();
          if( i + 1  != pipelines.size() )
          {
            oss << " && ";
          }
        }
        oss << ")";
        return oss.str();
      }

      //------------------------------------------------------------------------
      //! Set policy to `All` (default)
      //!
      //! All operations need to succeed in order for the parallel operation to
      //! be successful.
      //------------------------------------------------------------------------
      ParallelOperation<HasHndl> All()
      {
        policy.reset( new AllPolicy() );
        return std::move( *this );
      }

      //------------------------------------------------------------------------
      //! Set policy to `Any`
      //!
      //! Just one of the operations needs to succeed in order for the parallel
      //! operation to be successful.
      //------------------------------------------------------------------------
      ParallelOperation<HasHndl> Any()
      {
        policy.reset( new AnyPolicy( pipelines.size() ) );
        return std::move( *this );
      }

      //------------------------------------------------------------------------
      // Set policy to `Some`
      //!
      //! n (user defined) operations need to succeed in order for the parallel
      //! operation to be successful.
      //------------------------------------------------------------------------
      ParallelOperation<HasHndl> Some( size_t threshold )
      {
        policy.reset( new SomePolicy( pipelines.size(), threshold ) );
        return std::move( *this );
      }

      //------------------------------------------------------------------------
      //! Set policy to `At Least`.
      //!
      //! At least n (user defined) operations need to succeed in order for the
      //! parallel operation to be successful (the user handler will be called
      //! only when all operations are resolved).
      //------------------------------------------------------------------------
      ParallelOperation<HasHndl> AtLeast( size_t threshold )
      {
        policy.reset( new AtLeastPolicy( pipelines.size(), threshold ) );
        return std::move( *this );
      }

    private:

      //------------------------------------------------------------------------
      //! `All` policy implementation
      //!
      //! All operations need to succeed in order for the parallel operation to
      //! be successful.
      //------------------------------------------------------------------------
      struct AllPolicy : public PolicyExecutor
      {
        bool Examine( const XrdCl::XRootDStatus &status )
        {
          if( status.IsOK() ) return false;
          // we require all request to succeed
          return true;
        }
      };

      //------------------------------------------------------------------------
      //! `Any` policy implementation
      //!
      //! Just one of the operations needs to succeed in order for the parallel
      //! operation to be successful.
      //------------------------------------------------------------------------
      struct AnyPolicy : public PolicyExecutor
      {
        AnyPolicy( size_t size) : cnt( size )
        {
        }

        bool Examine( const XrdCl::XRootDStatus &status )
        {
          // decrement the counter
          size_t nb = cnt.fetch_sub( 1 );
          // we require just one operation to be successful
          if( status.IsOK() ) return true;
          // lets see if this is the last one?
          if( nb == 1 ) return true;
          // we still have a chance there will be one that is successful
          return false;
        }

        private:
          std::atomic<size_t> cnt;
      };

      //------------------------------------------------------------------------
      //! `Some` policy implementation
      //!
      //! n (user defined) operations need to succeed in order for the parallel
      //! operation to be successful.
      //------------------------------------------------------------------------
      struct SomePolicy : PolicyExecutor
      {
        SomePolicy( size_t size, size_t threshold ) : cnt( size ), succeeded( 0 ), threshold( threshold )
        {
        }

        bool Examine( const XrdCl::XRootDStatus &status )
        {
          // decrement the counter
          size_t nb = cnt.fetch_sub( 1 );
          if( status.IsOK() )
          {
            size_t s = succeeded.fetch_add( 1 );
            if( s + 1 == threshold ) return true; // we reached the threshold
            // we are not yet there
            return false;
          }
          // did we dropped bellow the threshold
          if( nb == threshold ) return true;
          // we still have a chance there will be enough of successful operations
          return false;
        }

        private:
          std::atomic<size_t> cnt;
          std::atomic<size_t> succeeded;
          const size_t        threshold;
      };

      //------------------------------------------------------------------------
      //! `At Least` policy implementation
      //!
      //! At least n (user defined) operations need to succeed in order for the
      //! parallel operation to be successful (the user handler will be called
      //! only when all operations are resolved).
      //------------------------------------------------------------------------
      struct AtLeastPolicy : PolicyExecutor
      {
        AtLeastPolicy( size_t size, size_t threshold ) : cnt( size ), threshold( threshold )
        {
        }

        bool Examine( const XrdCl::XRootDStatus &status )
        {
          // decrement the counter
          size_t nb = cnt.fetch_sub( 1 );
          // although we might have the minimum to succeed we wait for the rest
          if( status.IsOK() ) return false;
          if( nb == threshold ) return true; // we dropped bellow the threshold
          // we still have a chance there will be enough of successful operations
          return false;
        }

        private:
          std::atomic<size_t> cnt;
          const size_t        threshold;
      };

      //------------------------------------------------------------------------
      //! Helper class for handling the PipelineHandler of the
      //! ParallelOperation (RAII).
      //!
      //! Guarantees that the handler will be executed exactly once.
      //------------------------------------------------------------------------
      struct Ctx
      {
        //----------------------------------------------------------------------
        //! Constructor.
        //!
        //! @param handler : the PipelineHandler of the Parallel operation
        //----------------------------------------------------------------------
        Ctx( PipelineHandler *handler, PolicyExecutor  *policy  ): handler( handler ),
                                                                   policy( policy )
        {
        }

        //----------------------------------------------------------------------
        //! Destructor.
        //----------------------------------------------------------------------
        ~Ctx()
        {
          Handle( XRootDStatus() );
        }

        //----------------------------------------------------------------------
        //! Forwards the status to the PipelineHandler if the handler haven't
        //! been called yet.
        //!
        //! @param st : status
        //----------------------------------------------------------------------
        inline void Examine( const XRootDStatus &st )
        {
          if( policy->Examine( st ) )
            Handle( st );
        }

        //----------------------------------------------------------------------
        //! Forwards the status to the PipelineHandler if the handler haven't
        //! been called yet.
        //!
        //! @param st : status
        //---------------------------------------------------------------------
        inline void Handle( const XRootDStatus &st )
        {
            PipelineHandler* hdlr = handler.exchange( nullptr );
            if( hdlr )
              hdlr->HandleResponse( new XRootDStatus( st ), nullptr );
        }

        //----------------------------------------------------------------------
        //! PipelineHandler of the ParallelOperation
        //----------------------------------------------------------------------
        std::atomic<PipelineHandler*> handler;

        //----------------------------------------------------------------------
        //! Policy defining when the user handler should be called
        //----------------------------------------------------------------------
        std::unique_ptr<PolicyExecutor> policy;
      };

      //------------------------------------------------------------------------
      //! Run operation
      //!
      //! @param params :  container with parameters forwarded from
      //!                  previous operation
      //! @return       :  status of the operation
      //------------------------------------------------------------------------
      XRootDStatus RunImpl( PipelineHandler *handler, uint16_t pipelineTimeout )
      {
        // make sure we have a valid policy for the parallel operation
        if( !policy ) policy.reset( new AllPolicy() );

        std::shared_ptr<Ctx> ctx =
            std::make_shared<Ctx>( handler, policy.release() );

        uint16_t timeout = pipelineTimeout < this->timeout ?
                           pipelineTimeout : this->timeout;

        try
        {
          for( size_t i = 0; i < pipelines.size(); ++i )
          {
            pipelines[i].Run( timeout, [ctx]( const XRootDStatus &st ){ ctx->Examine( st ); } );
          }
        }
        catch( const PipelineException& ex )
        {
          return ex.GetError();
        }
        catch( const std::exception& ex )
        {
          return XRootDStatus( stError, ex.what() );
        }

        return XRootDStatus();
      }

      std::vector<Pipeline>           pipelines;
      std::unique_ptr<PolicyExecutor> policy;
  };

  //----------------------------------------------------------------------------
  //! Factory function for creating parallel operation from a vector
  //----------------------------------------------------------------------------
  template<class Container>
  inline ParallelOperation<false> Parallel( Container &container )
  {
    return ParallelOperation<false>( container );
  }

  //----------------------------------------------------------------------------
  //! Helper function for converting parameter pack into a vector
  //----------------------------------------------------------------------------
  inline void PipesToVec( std::vector<Pipeline>& )
  {
    // base case
  }

  //----------------------------------------------------------------------------
  // Declare PipesToVec (we need to do declare those functions ahead of
  // definitions, as they may call each other.
  //----------------------------------------------------------------------------
  template<typename ... Others>
  inline void PipesToVec( std::vector<Pipeline> &v, Operation<false> &operation,
      Others&... others );

  template<typename ... Others>
  inline void PipesToVec( std::vector<Pipeline> &v, Operation<true> &operation,
      Others&... others );

  template<typename ... Others>
  inline void PipesToVec( std::vector<Pipeline> &v, Pipeline &pipeline,
      Others&... others );

  //----------------------------------------------------------------------------
  // Define PipesToVec
  //----------------------------------------------------------------------------
  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<false> &operation,
      Others&... others )
  {
    v.emplace_back( operation );
    PipesToVec( v, others... );
  }

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Operation<true> &operation,
      Others&... others )
  {
    v.emplace_back( operation );
    PipesToVec( v, others... );
  }

  template<typename ... Others>
  void PipesToVec( std::vector<Pipeline> &v, Pipeline &pipeline,
      Others&... others )
  {
    v.emplace_back( std::move( pipeline ) );
    PipesToVec( v, others... );
  }

  //----------------------------------------------------------------------------
  //! Factory function for creating parallel operation from
  //! a given number of operations
  //! (we use && reference since due to reference collapsing this will fit
  //! both r- and l-value references)
  //----------------------------------------------------------------------------
  template<typename ... Operations>
  inline ParallelOperation<false> Parallel( Operations&& ... operations )
  {
    constexpr size_t size = sizeof...( operations );
    std::vector<Pipeline> v;
    v.reserve( size );
    PipesToVec( v, operations... );
    return Parallel( v );
  }
}

#endif // __XRD_CL_OPERATIONS_HH__
