/**
 * Copyright (c) 2017-2018, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "tc/core/check.h"

namespace tc {
namespace polyhedral {

/*
 * Extract a mapping from the domain elements active at "tree"
 * to identifiers "ids", where all branches in "tree"
 * are assumed to have been mapped to these identifiers.
 * The result lives in a space of the form "tupleId"["ids"...].
 */
template <typename MappingType>
isl::MultiUnionPwAff<Statement, MappingType> extractDomainToIds(
    const detail::ScheduleTree* root,
    const detail::ScheduleTree* tree,
    const std::vector<mapping::MappingId>& ids,
    isl::id tupleId) {
  using namespace polyhedral::detail;

  auto paramSpace = isl::Space<>(tree->ctx_, 0);
  auto empty = isl::UnionSet<Statement>::empty(paramSpace);
  auto space = paramSpace.add_named_tuple_id_ui<MappingType>(tupleId, ids.size());
  auto zero = isl::MultiVal<MappingType>::zero(space);
  auto domainToIds = isl::MultiUnionPwAff<Statement, MappingType>(empty, zero);

  for (auto mapping : tree->collect(tree, ScheduleTreeType::Mapping)) {
    auto mappingNode = mapping->as<ScheduleTreeMapping>();
    auto list = isl::UnionPwAffListOn<Statement>(tree->ctx_, ids.size());
    for (auto id : ids) {
      if (mappingNode->mapping.count(id) == 0) {
        break;
      }
      auto idMap = mappingNode->mapping.at(id);
      list = list.add(idMap);
    }
    // Ignore this node if it does not map to all required ids.
    if (static_cast<size_t>(list.size()) != ids.size()) {
      continue;
    }
    auto nodeToIds = isl::MultiUnionPwAff<Statement, MappingType>(space, list);
    auto active = isl::UnionSet<Statement>(activeDomainPoints(root, mapping));
    TC_CHECK(active.intersect(domainToIds.domain()).is_empty())
        << "conflicting mappings; are the filters in the tree disjoint?";
    nodeToIds = nodeToIds.intersect_domain(active);
    domainToIds = domainToIds.union_add(nodeToIds);
  }

  auto active = activeDomainPoints(root, tree);
  TC_CHECK(active.is_subset(domainToIds.domain()))
      << "not all domain points of\n"
      << active << "\nwere mapped to the required ids";

  return domainToIds;
}

} // namespace polyhedral
} // namespace tc
