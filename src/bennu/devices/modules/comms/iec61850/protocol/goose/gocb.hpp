/**
   @brief This file describes the GOOSE Control Block Class
   as defined in IEC61850-7-2 (Table 42)
*/
#ifndef __IEC61850_GOOSE_GOCB_HPP__
#define __IEC61850_GOOSE_GOCB_HPP__

/* stl includes */
#include <algorithm>
#include <cstdint>
#include <string>

#include "bennu/devices/modules/comms/iec61850/protocol/goose/data-set.hpp"

namespace ccss_protocols {
  namespace iec61850 {
    namespace goose {

      using std::string;
      using std::uint8_t;
      using std::uint32_t;

      class gocb {
      public:
        gocb() :
          GoCBName_(""), GoCBRef_(""), goEna_(true), goID_(""),
          DatSet_(""), ConfRev_(0), NdsCom_(false), DstAddress_(), dset() {
          DstAddress_[0] = 0xFF; DstAddress_[1] =0xFF; DstAddress_[2] = 0xFF;
          DstAddress_[3] = 0xFF; DstAddress_[4] =0xFF; DstAddress_[5] = 0xFF;
        }

        gocb(string gocb_name) :
          GoCBName_(gocb_name), GoCBRef_(""), goEna_(true),
          goID_(""), DatSet_(""), ConfRev_(0), NdsCom_(false),
          DstAddress_(), dset() {
          DstAddress_[0] = 0xFF; DstAddress_[1] =0xFF; DstAddress_[2] = 0xFF;
          DstAddress_[3] = 0xFF; DstAddress_[4] =0xFF; DstAddress_[5] = 0xFF;
        }

        gocb(string gocb_name, string dset_name) :
          GoCBName_(gocb_name), GoCBRef_(""), goEna_(true),
          goID_(""), DatSet_(""), ConfRev_(0), NdsCom_(false),
          DstAddress_(), dset(dset_name) {
          DstAddress_[0] = 0xFF; DstAddress_[1] =0xFF; DstAddress_[2] = 0xFF;
          DstAddress_[3] = 0xFF; DstAddress_[4] =0xFF; DstAddress_[5] = 0xFF;
        }

        string GoCBName( void ) { return GoCBName_; }
        void GoCBName( string name ) { GoCBName_ = name; }

        string GoCBRef( void ) { return GoCBRef_; }
        void GoCBRef( string reference ) { GoCBRef_ = reference; }

        bool goEna( void ) { return goEna_; }
        void goEna( bool enable ) { goEna_ = enable; }

        string goID( void ) { return goID_; }
        void goID( string id ) { goID_ = id; }

        string DatSet( void ) { return DatSet_; }
        void DatSet( string dsetRef) { DatSet_ = dsetRef; dset.reference(dsetRef); }

        uint32_t ConfRev( void ) { return ConfRev_; }
        void ConfRev( uint32_t revisions ) { ConfRev_ = revisions; }

        bool NdsCom( void ) { return NdsCom_; }
        void NdsCom( bool needsCommissioning ) { NdsCom_ = needsCommissioning; }

        uint8_t* DstAddress( void ) { return DstAddress_; }
        void DstAddress( uint8_t* destAddr) { std::copy(destAddr, destAddr+6, DstAddress_); }

      private:
        /* GOOSE Control Block Data */
        string   GoCBName_;      /**<Name for this instance of GoCB*/
        string   GoCBRef_;       /**<Path-name of this instance of GoCB*/
        bool     goEna_;         /**<Indicates that this GoCB may send GOOSE messages*/
        string   goID_;          /**<Identification for the GOOSE message*/
        string   DatSet_;        /**<Dataset reference for whose values shall be transmitted*/
        uint32_t ConfRev_;       /**<Number of times the dataset's configuration changed*/
        bool     NdsCom_;        /**<TRUE if GoCB needs further configuration*/
        uint8_t  DstAddress_[6]; /**<Destination MAC address*/

      public:
        data_set dset;           /**<Dataset managed by this GoCB*/
      };

    } // namespace goose

  } // namespace iec61850

} // namespace ccss_protocols

#endif /* __IEC61850_GOOSE_GOCB_HPP__ */
