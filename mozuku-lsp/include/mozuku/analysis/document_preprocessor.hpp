#pragma once

#include "comment_extractor.hpp"
#include "mozuku/core/types.hpp"

#include <string>
#include <vector>

namespace MoZuku::analysis {

struct ProcessedDocument {
  std::string analysisText;
  std::vector<comments::CommentSegment> commentSegments;
  std::vector<ByteRange> contentHighlightRanges;
};

class DocumentPreprocessor {
public:
  ProcessedDocument prepare(const std::string &languageId,
                            const std::string &text) const;
};

} // namespace MoZuku::analysis
