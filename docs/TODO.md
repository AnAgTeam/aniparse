# Future work and current boundaries

This page separates open work from the API available today. It is a backlog,
not a release schedule. The [README](../README.md) describes current capabilities;
[architecture](architecture.md) describes their contracts.

## Open work

- [ ] Define a reusable HTTP cache policy, including ETag revalidation. Existing
  resource/metadata caches and platform transport caching are separate mechanisms.
- [ ] Design a general streaming-download API for large resources. The current
  `ResourceAdapter`/`ResourceHandle` contracts support consumer-driven resource
  processing; the consumer still owns streaming sessions, cancellation and
  bounded output queues. See [media transport ownership](architecture.md#media-transport-ownership).
- [ ] Package standalone Swift bindings with an independent integration guide.
  The application has a Swift facade and bridge; those are not a separately
  distributed binding package in this repository.
- [ ] Publish a vcpkg port for the library itself. Building the repository with
  vcpkg-managed dependencies already works.

## Existing functionality that was previously listed as future work

- Transport retries are configurable through `HttpClientConfig::max_retries` in
  both curl and NSURLSession backends. Cancellation and HTTP error statuses are
  not transport failures to retry under that contract.
- The Apple NSURLSession backend is implemented and selected with
  `ANIPARSE_NSURLSESSION_BACKEND`.
- A parser-writing guide and [independent runnable examples](../examples/README.md)
  cover minimal sources, authentication, mixed catalogs, filters and related URLs.

## Boundaries to preserve in descriptions and examples

**Source updates.** Signed overrides replace settings that implementations expose:
registered domains, profile origins, selectors and patterns. They do not replace
parser logic. A changed protocol or an unsupported response structure can require
new compiled code. The core does not download executable parser extensions.

**Source development.** Shared request handling and parsing helpers reduce repeated
work. Source implementations still need correct identity, pagination, error
handling and fixtures; authentication, dynamic pages or media protocols can need
substantial additional work. The minimal template demonstrates the contract,
not an estimate of the effort needed for every source.

**Capabilities and failures.** Capability metadata describes supported operations
and inputs. It does not guarantee that a request succeeds or that all optional
methods are implemented. Consumers must handle operation errors, including
`NotImplemented`, independently of UI capability summaries.

**Identity and relations.** External IDs and related-work descriptors carry
source-provided evidence. Automatic record matching, merging and recovery are
consumer policy, not guarantees of the core. Cross-media relations identify
different works, and a destination URL must be routed with the receiving
catalog's own profile.

**Media consumption.** Sources may offer metadata only. The library's media models,
extractors and adapters do not by themselves supply a reader UI, video player,
download manager or streaming proxy. Those integrations belong to consumers.
