#include "Parser.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

#include <boost/filesystem.hpp>

#include "bennu/loaders/DynamicLibraryLoader.hpp"
#include "bennu/loaders/PathFinder.hpp"
#include "bennu/parsers/ParserPropertyTree.hpp"

using namespace bennu::parsers;
using namespace bennu::loaders;


Parser::Parser() :
    mPathFinder( new PathFinder )
{
    mPathFinder->addPath( "." );
    mPathFinder->addPath( ".." );
    mPathFinder->addPath( "/etc/sceptre/" );
    mPathFinder->addPath( "/usr/share" );
    mPathFinder->addPath( "/usr/local/share" );
}


bool Parser::load( const std::string& filename )
{
    // find extension
    std::string extension;
    size_t found = filename.find_last_of( '.' );
    if ( found == std::string::npos )
    {
        std::cerr << "ERROR: There is no extension on the file!" << std::endl;
        return false;
    }
    extension = filename.substr( found + 1 );

    std::map<std::string, std::shared_ptr<ParserPropertyTree> >::iterator iter;
    iter = mParsers.find( extension );
    if ( iter == mParsers.end() )
    {
        std::cerr << "ERROR: No parser has been registered for extension " << extension << "!" << std::endl;
        return false;
    }

    std::string fullFilename = mPathFinder->getPathForFilename( filename );
    if ( fullFilename == "" )
    {
        std::cerr << "ERROR: Parser failed to find the file \"" << filename << "\" at any of the default locations!" << std::endl;
        return false;
    }

    std::cerr << "INFO: Parser changed file name: \"" << filename << "\" --> \"" << fullFilename << "\"" << std::endl;

    boost::property_tree::ptree parserTree;
    if ( !iter->second->load( fullFilename, parserTree ) )
    {
        std::cerr << "ERROR: Parser for \"" << fullFilename << "\" failed!" << std::endl;
        return false;
    }

    const boost::property_tree::ptree& tree = parserTree.front().second;
    parseAndLoadLibraries( tree);
    //Now that libraries are loaded, parse the data.
    BOOST_FOREACH( const boost::property_tree::ptree::value_type& v, tree )
    {
        if ( isTreeDataHandlerRegistered( v.first ) )
        {
            std::vector<TreeDataHandler> tdh = getRegisteredHandlers( v.first );
            for ( size_t i = 0; i < tdh.size(); ++i )
            {
                if ( tdh[i] )
                {
                    tdh[i]( v.first, v.second );
                }
            }
        }

    }

    return true;


}


void Parser::parseAndLoadLibraries(const boost::property_tree::ptree& tree)
{
    BOOST_FOREACH( const boost::property_tree::ptree::value_type& v, tree )
    {
        if ( isTagRegisteredForDynamicLibrary( v.first ) )
        {
            std::string library = getDynamicLibraryForTag( v.first );
            DynamicLibraryLoader::the()->load( library );
        }
        parseAndLoadLibraries(v.second);
    }
}


void Parser::write( const boost::property_tree::ptree& tree )
{
    std::cout << std::endl;
    boost::archive::text_oarchive oa( std::cout );
    oa << tree;
    boost::property_tree::save( oa, tree, 1 );
}


bool Parser::save( const std::string& directory, const std::string& filename, const boost::property_tree::ptree& tree )
{
    if ( !boost::filesystem::is_directory( directory ) )
    {
        if ( !boost::filesystem::create_directory( directory ) )
        {
            std::cerr << "ERROR: Unable to create directory \"" << directory << "\"!" << std::endl;
            return false;
        }
    }

    return save( directory + "/" + filename, tree );
}


bool Parser::save( const std::string& filename, const boost::property_tree::ptree& tree )
{
    // find extension
    std::string extension;
    size_t found = filename.find_last_of( '.' );
    if ( found == std::string::npos )
    {
        std::cerr << "ERROR: There is no extension on the file \"" << filename << "\"!" << std::endl;
        return false;
    }

    extension = filename.substr( found + 1 );

    std::map<std::string, std::shared_ptr<ParserPropertyTree> >::iterator iter;
    iter = mParsers.find( extension );
    if ( iter == mParsers.end() )
    {
        std::cerr << "ERROR: No parser has been registered for extension " << extension << "!" << std::endl;
        return false;
    }

    return iter->second->save( filename, tree );
}


void Parser::registerParser( const std::string& extension, std::shared_ptr<ParserPropertyTree> parser )
{
    mParsers[extension] = parser;
}
