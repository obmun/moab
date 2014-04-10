/*
 * NCWriteMPAS.hpp
 *
 *  nc write helper for MPAS type data (CAM)
 *  Created on: April 9, 2014
 *
 */

#ifndef NCWRITEMPAS_HPP_
#define NCWRITEMPAS_HPP_

#include "NCWriteHelper.hpp"

namespace moab {

class NCWriteMPAS: public NCWriteHelper
{
public:
  NCWriteMPAS(WriteNC* writeNC, int fileId, const FileOptions& opts, EntityHandle fileSet) :
    NCWriteHelper(writeNC, fileId, opts, fileSet) {}

  virtual ~NCWriteMPAS();

private:
  ErrorCode write_values(std::vector<std::string>& var_names, EntityHandle fileSet);
};

} // namespace moab

#endif
