#include <memory>
#include <string>

#include "envoy/http/filter.h"
#include "envoy/http/header_map.h"
#include "envoy/registry/registry.h"
#include "envoy/server/filter_config.h"

#include "common/stats/symbol_table_impl.h"

#include "extensions/filters/http/common/empty_http_filter_config.h"
#include "extensions/filters/http/common/pass_through_filter.h"

namespace Envoy {

// A test filter that rejects all requests if EDS isn't healthy yet, and
// responds OK to all requests if it is.
class EdsReadyFilter : public Http::PassThroughFilter {
public:
  EdsReadyFilter(const Stats::Scope& root_scope)
      : root_scope_(root_scope),
        stat_name_("cluster.cluster_0.membership_healthy",
                   const_cast<Stats::SymbolTable&>(root_scope_.constSymbolTable())) {}
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap&, bool) override {
    absl::optional<std::reference_wrapper<const Stats::Gauge>> gauge =
        root_scope_.findGauge(stat_name_.statName());
    if (!gauge.has_value()) {
      decoder_callbacks_->sendLocalReply(Envoy::Http::Code::InternalServerError,
                                         "Couldn't find stat", nullptr, absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
    if (gauge->get().value() == 0) {
      decoder_callbacks_->sendLocalReply(Envoy::Http::Code::InternalServerError, "EDS not ready",
                                         nullptr, absl::nullopt, "");
      return Http::FilterHeadersStatus::StopIteration;
    }
    decoder_callbacks_->sendLocalReply(Envoy::Http::Code::OK, "EDS is ready", nullptr,
                                       absl::nullopt, "");
    return Http::FilterHeadersStatus::StopIteration;
  }

private:
  const Stats::Scope& root_scope_;
  Stats::StatNameManagedStorage stat_name_;
};

class EdsReadyFilterConfig : public Extensions::HttpFilters::Common::EmptyHttpFilterConfig {
public:
  EdsReadyFilterConfig() : EmptyHttpFilterConfig("eds-ready-filter") {}

  Http::FilterFactoryCb
  createFilter(const std::string&,
               Server::Configuration::FactoryContext& factory_context) override {
    return [&factory_context](Http::FilterChainFactoryCallbacks& callbacks) {
      callbacks.addStreamFilter(
          std::make_shared<EdsReadyFilter>(factory_context.api().rootScope()));
    };
  }
};

static Registry::RegisterFactory<EdsReadyFilterConfig,
                                 Server::Configuration::NamedHttpFilterConfigFactory>
    register_;

} // namespace Envoy
