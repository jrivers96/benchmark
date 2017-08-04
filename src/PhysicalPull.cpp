/*
**
* BEGIN_COPYRIGHT
*
* Copyright (C) 2008-2016 SciDB, Inc.
* All Rights Reserved.
*
* Pull is a plugin for SciDB, an Open Source Array DBMS maintained
* by Paradigm4. See http://www.paradigm4.com/
*
* Pull is free software: you can redistribute it and/or modify
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

#include <limits>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <ctype.h>
#include <query/TypeSystem.h>
#include <query/Operator.h>
#include <log4cxx/logger.h>
#include "PullSettings.h"
#include "MemChunkBuilder.h"

#include <query/TypeSystem.h>
#include <query/FunctionDescription.h>
#include <query/FunctionLibrary.h>
#include <query/Operator.h>
#include <query/TypeSystem.h>
#include <query/FunctionLibrary.h>
#include <query/Operator.h>
#include <array/DBArray.h>
#include <array/Tile.h>
#include <array/TileIteratorAdaptors.h>

//#include <array/PinBuffer.h>


#include <util/Network.h>
#include <query/Operator.h>
#include <util/Platform.h>
#include <array/Tile.h>

#include <system/Sysinfo.h>
#include <util/Network.h>

#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>

using std::shared_ptr;
using std::make_shared;
using namespace std;

namespace scidb
{

using namespace scidb;

static size_t getChunkOverheadSize()
{
    return             (  sizeof(ConstRLEPayload::Header) +
                                 2 * sizeof(ConstRLEPayload::Segment) +
                                 sizeof(varpart_offset_t) + 5);
}

static size_t getSizeOffset()
{
    return getChunkOverheadSize()-4;
}

class PhysicalPull : public PhysicalOperator
{
public:
	PhysicalPull(std::string const& logicalName,
        std::string const& physicalName,
        Parameters const& parameters,
        ArrayDesc const& schema):
            PhysicalOperator(logicalName, physicalName, parameters, schema)
{}

 virtual bool changesDistribution(std::vector<ArrayDesc> const&) const
 {
     return true;
 }

 virtual RedistributeContext getOutputDistribution(
            std::vector<RedistributeContext> const& inputDistributions,
            std::vector< ArrayDesc> const& inputSchemas) const
 {
     return RedistributeContext(createDistribution(psUndefined), _schema.getResidency() );
 }

std::shared_ptr< Array> execute(std::vector< std::shared_ptr< Array> >& inputArrays, std::shared_ptr<Query> query)
{
    shared_ptr<Array>& inputArray = inputArrays[0];
    ArrayDesc const& inputSchema = inputArray->getArrayDesc();
    pull::Settings settings(inputSchema, _parameters, false, query);
    size_t const numInputAtts= settings.numInputAttributes();
    vector<string> attNames(numInputAtts);
    vector<shared_ptr<ConstArrayIterator> > iaiters(numInputAtts);
    vector<shared_ptr<ConstChunkIterator> > iciters(numInputAtts);
    for(size_t i =0; i<numInputAtts; ++i)
    {
        attNames[i] = inputSchema.getAttributes()[i].getName();
        iaiters[i] = inputArray->getConstIterator(i);
    }

    pull::InstanceSummary summary(query->getInstanceID(), numInputAtts, attNames);

    for(AttributeID i=0; i<numInputAtts; ++i)
    {
        size_t bytesWritten = 0;
        std::clock_t c_start = std::clock();
        auto t_start = std::chrono::high_resolution_clock::now();
        while(!iaiters[i]->end())
        {
            //ConstChunk const& chunk = iaiters[i]->getChunk();
            iciters[i] = iaiters[i]->getChunk().getConstIterator(ConstChunkIterator::IGNORE_OVERLAPS | ConstChunkIterator::IGNORE_EMPTY_CELLS);
            ConstChunk const& ch = iciters[i]->getChunk();
            PinBuffer pinScope(ch);
            uint32_t sourceSize = ch.getSize();
            //uint32_t* sizePointer = (uint32_t*) (((char*)ch.getData()) + MemChunkBuilder::chunkSizeOffset());
            //uint32_t const sourceSize = *((uint32_t*)(((char*) ch.getData()) + getSizeOffset()));
            bytesWritten += sourceSize;
            /*
            char *source = ch.getConstData();

            char *dest;
            std::memcpy(dest, source, sizeof dest);
            */
            //summary.addChunkData(i, chunk.getSize(), chunk.count());
            ++(*iaiters[i]);
            //LOG4CXX_DEBUG(logger, std::setprecision(2) << "bytes written:" << bytesWritten);

        }
        std::clock_t c_end = std::clock();
        double elapsed = 1000.0 *(c_end-c_start) / CLOCKS_PER_SEC;
        auto t_end = std::chrono::high_resolution_clock::now();
        double highres = std::chrono::duration<double, std::milli>(t_end-t_start).count();
        LOG4CXX_DEBUG(logger, std::setprecision(4) << "time foo1:" << elapsed);
        LOG4CXX_DEBUG(logger, std::setprecision(4) << "time foo2:" << highres);

        summary.addChunkData(i, bytesWritten, elapsed );

        /*std::cout << std::fixed << std::setprecision(2) << "CPU time used: "
                   << 1000.0 * (c_end-c_start) / CLOCKS_PER_SEC << " ms\n"
                   << "Wall clock time passed: "
                   << std::chrono::duration<double, std::milli>(t_end-t_start).count()
                   << " ms\n";
        */
    }


    summary.makeFinalSummary(settings, _schema, query);
    return summary.toArray(settings, _schema, query);
}
};

REGISTER_PHYSICAL_OPERATOR_FACTORY(PhysicalPull, "pull", "PhysicalPull");


} // end namespace scidb
