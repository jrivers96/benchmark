/*
**
* BEGIN_COPYRIGHT
*
* Copyright (C) 2008-2016 SciDB, Inc.
* All Rights Reserved.
*
* summarize is a plugin for SciDB, an Open Source Array DBMS maintained
* by Paradigm4. See http://www.paradigm4.com/
*
* summarize is free software: you can redistribute it and/or modify
* it under the terms of the AFFERO GNU General Public License as published by
* the Free Software Foundation.
*
* summarize is distributed "AS-IS" AND WITHOUT ANY WARRANTY OF ANY KIND,
* INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY,
* NON-INFRINGEMENT, OR FITNESS FOR A PARTICULAR PURPOSE. See
* the AFFERO GNU General Public License for the complete license terms.
*
* You should have received a copy of the AFFERO GNU General Public License
* along with summarize.  If not, see <http://www.gnu.org/licenses/agpl-3.0.html>
*
* END_COPYRIGHT
*/

#ifndef PULL_SETTINGS
#define PULL_SETTINGS

#include <limits>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <ctype.h>

#include <query/Operator.h>
#include <query/AttributeComparator.h>
#include <util/Platform.h>
#include <util/Network.h>

#include <boost/algorithm/string.hpp>
#include <boost/unordered_map.hpp>
#include <boost/lexical_cast.hpp>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/vector.hpp>

namespace scidb
{

static log4cxx::LoggerPtr logger(log4cxx::Logger::getLogger("scidb.pull"));

namespace pull
{

using std::string;
using std::vector;
using std::shared_ptr;
using std::dynamic_pointer_cast;
using std::ostringstream;
using boost::algorithm::trim;
using boost::starts_with;
using boost::lexical_cast;
using boost::bad_lexical_cast;

/*
 * Settings for the pull operator.
 */
class Settings
{
private:
    size_t _numInputAttributes;
    size_t _numInstances;
    bool _perAttributeSet;
    bool _perAttribute;
    bool _perInstanceSet;
    bool _perInstance;

public:
    static const size_t MAX_PARAMETERS = 2;
    Settings(ArrayDesc const& inputSchema,
             vector< shared_ptr<OperatorParam> > const& operatorParameters,
             bool logical,
             shared_ptr<Query>& query):
        _numInputAttributes(inputSchema.getAttributes().size()),
        _numInstances(query->getInstancesCount()),
        _perAttributeSet(false),
        _perAttribute(false),
        _perInstanceSet(false),
        _perInstance(false)
    {
        string const perAttributeParamHeader              = "per_attribute=";
        string const perInstanceParamHeader              = "per_instance=";
        size_t const nParams = operatorParameters.size();
         if (nParams > MAX_PARAMETERS)
         {   //assert-like exception. Caller should have taken care of this!
             throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION)
                   << "illegal number of parameters passed to Settings";
         }
        for(size_t i = 0; i<operatorParameters.size(); ++i)
    	{
            shared_ptr<OperatorParam>const& param = operatorParameters[i];
            {
                string parameterString;
                if (logical)
                {
                    parameterString = evaluate(((shared_ptr<OperatorParamLogicalExpression>&) param)->getExpression(),query, TID_STRING).getString();
                }
                else
                {
                    parameterString = ((shared_ptr<OperatorParamPhysicalExpression>&) param)->getExpression()->evaluate().getString();
                }
                parseStringParam(parameterString);
            }
    	}
    }
private:

    bool checkBoolParam(string const& param, string const& header, bool& target, bool& setFlag)
    {
        string headerWithEq = header + "=";
        if(starts_with(param, headerWithEq))
        {
            if(setFlag)
            {
                ostringstream error;
                error<<"illegal attempt to set "<<header<<" multiple times";
                throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << error.str().c_str();
            }
            string paramContent = param.substr(headerWithEq.size());
            trim(paramContent);
            try
            {
                target = lexical_cast<bool>(paramContent);
                setFlag = true;
                return true;
            }
            catch (bad_lexical_cast const& exn)
            {
                ostringstream error;
                error<<"could not parse "<<param.c_str();
                throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << error.str().c_str();
            }
        }
        return false;
    }
    void parseStringParam(string const& param)
    {
        if(checkBoolParam (param,   "per_attribute",       _perAttribute,          _perAttributeSet       ) ) { return; }
        if(checkBoolParam (param,   "per_instance",       _perInstance,          _perInstanceSet       ) ) { return; }
        ostringstream error;
        error<<"unrecognized parameter "<<param;
        throw SYSTEM_EXCEPTION(SCIDB_SE_INTERNAL, SCIDB_LE_ILLEGAL_OPERATION) << error.str().c_str();
    }
public:
    ArrayDesc getSchema(shared_ptr<Query>& query)
    {
        vector<DimensionDesc> dimensions(2);
        dimensions[0] = DimensionDesc("inst",  0, 0, _numInstances-1,       _numInstances-1,       1,                   0);
        dimensions[1] = DimensionDesc("attid", 0, 0, _numInputAttributes-1, _numInputAttributes-1, _numInputAttributes, 0);
        vector<AttributeDesc> attributes;
        attributes.push_back(AttributeDesc((AttributeID) 0, "att",    TID_STRING, AttributeDesc::IS_NULLABLE, 0));
        attributes.push_back(AttributeDesc((AttributeID) 1, "read_bytes", TID_UINT64, AttributeDesc::IS_NULLABLE, 0));
        attributes.push_back(AttributeDesc((AttributeID) 2, "total_seconds", TID_DOUBLE, AttributeDesc::IS_NULLABLE, 0));
        attributes.push_back(AttributeDesc((AttributeID) 3, "bytes_per_second",  TID_DOUBLE, AttributeDesc::IS_NULLABLE, 0));
        attributes = addEmptyTagAttribute(attributes);
        return ArrayDesc("pull", attributes, dimensions, defaultPartitioning(), query->getDefaultArrayResidency());
    }

    static const size_t NUM_OUTPUT_ATTRIBUTES = 4;

    size_t numInputAttributes() const
    {
        return _numInputAttributes;
    }
    bool perAttributeflag() const
    {
        return _perAttribute;
    }
    bool perInstanceflag() const
    {
        return _perInstance;
    }

};

struct SummaryTuple
{
private:
  friend class boost::serialization::access;
  template<class Archive>
  void serialize(Archive & ar, const unsigned int /*version*/)
  {
	   ar & attName;
	   ar & readBytes;
	   ar & totalSeconds;
	   ar & bytesPerSecond;
  }
public:
	string attName;
    ssize_t readBytes;
    double totalSeconds;
    double bytesPerSecond;

    SummaryTuple(string att = ""):
        attName(att),
        readBytes(0),
        totalSeconds(0),
        bytesPerSecond(0)
    {}
};

struct InstanceSummary
{
    InstanceID myInstanceId;
    vector<SummaryTuple> summaryData;
    InstanceSummary(InstanceID iid,
                    size_t const numAttributes,
                    vector<string> attNames):
        myInstanceId(iid),
        summaryData(numAttributes,SummaryTuple())
    {
        for(size_t i =0; i<numAttributes; ++i)
        {
            summaryData[i].attName=attNames[i];
        }
    }

    void addChunkData(AttributeID attId, ssize_t attBytes, double attSeconds)
    {
        SummaryTuple& tuple = summaryData[attId];
        tuple.readBytes+=attBytes;
        tuple.totalSeconds+=attSeconds;
    }

    bool makeFinalSummary(Settings const&settings, ArrayDesc const& schema, shared_ptr<Query>& query)
    {
        size_t numAtt = Settings::NUM_OUTPUT_ATTRIBUTES;
        InstanceID const myId     = query->getInstanceID();
        InstanceID const coordId  = query->getCoordinatorID() == INVALID_INSTANCE ? myId : query->getCoordinatorID();
        size_t const numInstances = query->getInstancesCount();
        //bool const perAtt = settings.perAttributeflag();
        //bool const perIns = settings.perInstanceflag();
        bool const perAtt = settings.perAttributeflag();
        bool const perIns = settings.perAttributeflag();;
        if(perAtt==false && perIns==false)
        {
            if(myId != coordId)
            {
                std::stringstream out;
                boost::archive::binary_oarchive oa(out);
                oa << summaryData;
                out.seekg(0, std::ios::end);
                size_t bufSize = out.tellg();
                out.seekg(0,std::ios::beg);
                auto tmp = out.str();
                const char* cstr = tmp.c_str();
                shared_ptr<SharedBuffer> bufsend(new MemoryBuffer(cstr, bufSize));
                BufSend(coordId, bufsend, query);
                summaryData.clear();
            }
            else
            {
                SummaryTuple globalSummary("all");
                for(size_t att =0; att<summaryData.size(); ++att)
                {
                    SummaryTuple& t = summaryData[att];
                    /*if(att==0)
                    {
                        globalSummary.totalCount += t.totalCount;
                    }
                    */
                    globalSummary.totalSeconds += t.totalSeconds;
                    globalSummary.readBytes += t.readBytes;
                }
                for(InstanceID i = 0; i<numInstances; ++i)
                {
                    if (i == myId)
                    {
                        continue;
                    }
                    shared_ptr<SharedBuffer> buf = BufReceive(i, query);
                    std::string bufstring((const char *)buf->getConstData(), buf->getSize());
                    std::stringstream ssout;
                    ssout << bufstring;
                    boost::archive::binary_iarchive ia(ssout);
                    std::vector<SummaryTuple> newlist;
                    ia >> newlist;
                    std::vector<SummaryTuple>::size_type sz = newlist.size();
                    for (unsigned att=0; att<sz; att++)
                    {
                        SummaryTuple& t = newlist[att];
                        /*if(att==0)
                        {
                            globalSummary.totalCount += t.totalCount;
                        }
                        */
                        globalSummary.totalSeconds += t.totalSeconds;
                        globalSummary.readBytes += t.readBytes;
                    }
                }
                summaryData.clear();
                summaryData.push_back(globalSummary);
            }
        }
        else if(perAtt && !perIns)
        {
            if(myId != coordId)
            {
                std::stringstream out;
                boost::archive::binary_oarchive oa(out);
                oa << summaryData;
                out.seekg(0, std::ios::end);
                size_t bufSize = out.tellg();
                out.seekg(0,std::ios::beg);
                auto tmp = out.str();
                const char* cstr = tmp.c_str();
                shared_ptr<SharedBuffer> bufsend(new MemoryBuffer(cstr, bufSize));
                BufSend(coordId, bufsend, query);
                summaryData.clear();
            }
            else
            {
                for(InstanceID i = 0; i<numInstances; ++i)
                {
                    if (i == myId)
                    {
                        continue;
                    }
                    shared_ptr<SharedBuffer> buf = BufReceive(i, query);
                    std::string bufstring((const char *)buf->getConstData(), buf->getSize());
                    std::stringstream ssout;
                    ssout << bufstring;
                    boost::archive::binary_iarchive ia(ssout);
                    std::vector<SummaryTuple> newlist;
                    ia >> newlist;
                    std::vector<SummaryTuple>::size_type sz = newlist.size();
                    //TODO:assert that the coord vector and slave vectors are equal in size or error out.
                    SummaryTuple summary;
                    for (unsigned att=0; att<sz; att++)
                    {
                        summaryData[att].attName = newlist[att].attName;
                        summaryData[att].totalSeconds += newlist[att].totalSeconds;
                        summaryData[att].readBytes += newlist[att].readBytes;
                        //LOG4CXX_DEBUG(logger, std::setprecision(4) << "readbytes foo3:" << summaryData[att].readBytes);
                    }
                }
            }
        }
        else if (perIns && !perAtt) {
            std::vector<SummaryTuple>::size_type sz = summaryData.size();
            SummaryTuple instanceSummary("all");
            for (unsigned att = 0; att < sz; att++) {
                /*if(att == 0)
                 {
                 instanceSummary.totalCount = summaryData[att].totalCount;
                 if(instanceSummary.maxChunkCount < summaryData[att].maxChunkCount)
                 {
                 instanceSummary.maxChunkCount = summaryData[att].maxChunkCount;
                 }
                 if(instanceSummary.minChunkCount > summaryData[att].minChunkCount)
                 {
                 instanceSummary.minChunkCount = summaryData[att].minChunkCount;
                 }
                 }
                 */
                //instanceSummary.numChunks += summaryData[att].numChunks;
                //instanceSummary.totalBytes += summaryData[att].totalBytes;
                //instanceSummary[att].attName       = summaryData[att].attName;
                instanceSummary.totalSeconds += summaryData[att].totalSeconds;
                instanceSummary.readBytes += summaryData[att].readBytes;
            }
            summaryData.clear();
            summaryData.push_back(instanceSummary);
        }
                return true;
    }

    shared_ptr<Array> toArray(Settings const& settings,ArrayDesc const& schema, shared_ptr<Query>& query)
    {
        shared_ptr<Array> outputArray(new MemArray(schema, query));
        if (summaryData.size() == 0)
        {
            return outputArray;
        }
        InstanceID const myId    = query->getInstanceID();
        InstanceID const coordId = 0;
        Coordinates position(2,0);
        size_t numInstances    = query->getInstancesCount();
        position[0]=myInstanceId;
        vector<shared_ptr<ArrayIterator> > oaiters(Settings::NUM_OUTPUT_ATTRIBUTES);
        vector<shared_ptr<ChunkIterator> > ociters(Settings::NUM_OUTPUT_ATTRIBUTES);
        for(size_t oatt = 0; oatt<Settings::NUM_OUTPUT_ATTRIBUTES; ++oatt)
        {
            oaiters[oatt] = outputArray->getIterator(oatt);
            ociters[oatt] = oaiters[oatt]->newChunk(position).getIterator(query, oatt == 0 ?
                    ChunkIterator::SEQUENTIAL_WRITE :
                    ChunkIterator::NO_EMPTY_CHECK | ChunkIterator::SEQUENTIAL_WRITE);
        }

        Value buf;
        for(size_t i=0; i<summaryData.size(); ++i)
        {
            SummaryTuple const& t = summaryData[i];
            LOG4CXX_DEBUG(logger, std::setprecision(4) << "write array read bytes foo4:" << t.readBytes);
            LOG4CXX_DEBUG(logger, std::setprecision(4) << "write array seconds foo4:"    << t.totalSeconds);

            buf.setString(t.attName);
            ociters[0]->setPosition(position);
            ociters[0]->writeItem(buf);

            buf.reset<uint64_t>((uint64_t)t.readBytes);
            ociters[1]->setPosition(position);
            ociters[1]->writeItem(buf);

            buf.reset<double>((double)t.totalSeconds);
            ociters[2]->setPosition(position);
            ociters[2]->writeItem(buf);

            buf.setDouble(((double)t.readBytes)/((double)t.totalSeconds/1000.0));
            ociters[3]->setPosition(position);
            ociters[3]->writeItem(buf);
            position[1]++;
        }
        for(size_t oatt = 0; oatt<Settings::NUM_OUTPUT_ATTRIBUTES; ++oatt)
        {
            ociters[oatt]->flush();
        }
        return outputArray;
    }
};

} } //namespaces

#endif //pull_settings
