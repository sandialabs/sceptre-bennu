#ifndef bennu_PARSERS_PARSER
#define bennu_PARSERS_PARSER

#include <map>
#include <vector>

#include <boost/archive/basic_archive.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_serialization.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/foreach.hpp>

#include "bennu/utility/Singleton.hpp"

namespace bennu
{
    namespace loaders
    {
        class PathFinder;
    }

    namespace parsers
    {

        class ParserPropertyTree;


        typedef std::function<bool ( const std::string& element , const boost::property_tree::ptree& tree )> TreeDataHandler;


        class Parser : public utility::Singleton<Parser>
        {
        public:

            friend class utility::Singleton<Parser>;


        public:

            // Chooses the parser to use to parse the file
            bool load( const std::string& filename );//, std::string& error );

            void write( const boost::property_tree::ptree& tree );
            bool save( const std::string& directory, const std::string& filename, const boost::property_tree::ptree& tree );
            bool save( const std::string& filename, const boost::property_tree::ptree& tree );

            void registerTreeDataHandler( const std::string& element, TreeDataHandler tdh )
            {
                mHandlers.insert( std::make_pair( element, tdh ) );
            }

            void registerParser( const std::string& extension, std::shared_ptr<ParserPropertyTree> parser );

            std::vector<std::string> getRegisteredElements() const
            {
                std::vector<std::string> elements;
                for ( auto citer = mHandlers.begin(); citer != mHandlers.end(); ++citer )
                {
                    elements.push_back( citer->first );
                }

                return elements;
            }

            void registerTagForDynamicLibrary( const std::string& tag, const std::string& library )
            {
                mDynamicLibraryTags[tag] = library;
            }

            bool isTagRegisteredForDynamicLibrary( const std::string& tag )
            {
                return mDynamicLibraryTags.find( tag ) != mDynamicLibraryTags.end();
            }

            std::string getDynamicLibraryForTag( const std::string& tag ) const
            {
                auto iter = mDynamicLibraryTags.find( tag );
                if ( iter != mDynamicLibraryTags.end() )
                {
                    return iter->second;
                }

                return "";

            }


        protected:

            void parseAndLoadLibraries(const boost::property_tree::ptree& tree);


        private:

            Parser();
            virtual ~Parser() {}

            bool isTreeDataHandlerRegistered( const std::string& element ) const
            {
                return mHandlers.find( element ) != mHandlers.end();
            }

            bool isParserRegistered( const std::string& extension ) const
            {
                return mParsers.find( extension ) != mParsers.end();
            }


            std::vector<TreeDataHandler> getRegisteredHandlers( const std::string& element ) const
            {
                std::vector<TreeDataHandler> handlers;
                std::pair<std::multimap<std::string, TreeDataHandler>::const_iterator, std::multimap<std::string, TreeDataHandler>::const_iterator> iters;
                iters = mHandlers.equal_range( element );

                std::multimap<std::string, TreeDataHandler>::const_iterator citer;
                for ( citer = iters.first; citer != iters.second; ++citer )
                {
                    handlers.push_back( citer->second );
                }

                return handlers;
            }

            std::shared_ptr<loaders::PathFinder> mPathFinder;

            std::multimap<std::string, TreeDataHandler> mHandlers;
            std::map<std::string, std::string> mDynamicLibraryTags;
            std::map<std::string, std::shared_ptr<ParserPropertyTree> > mParsers;

        };

    }

}



#endif // bennu_PARSERS_JSONPARSERPROPERTYTREE_hpp
