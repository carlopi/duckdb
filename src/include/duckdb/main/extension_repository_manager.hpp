//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/main/extension_repository_manager.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/enums/on_create_conflict.hpp"
#include "duckdb/common/enums/on_entry_not_found.hpp"
#include "duckdb/main/extension_install_info.hpp"

namespace duckdb {
class ClientContext;
class DatabaseInstance;
class FileSystem;

//! Manages the trusted extension repositories of a DuckDB installation. Trusted repositories are stored as json files
//! in the extension repository directory (by default `~/.duckdb/extension_repositories`). Every repository has a
//! locally unique name, a url prefix from which extensions are downloaded and one or more public keys that are trusted
//! to sign the extensions that are served by the repository. The keys of a trusted repository are only trusted for the
//! extensions that are installed from that repository.
class ExtensionRepositoryManager {
public:
	//! The file extension used for the stored repository definitions
	//! Repository files carry a ".duckdb_extension." segment so that they are covered by the write protection in
	//! OpenerFileSystem::IsDuckDBExtensionName: they hold the pinned keys that decide which extensions may be loaded,
	//! so they must not be writable through anything but CREATE EXTENSION REPOSITORY
	static constexpr const char *FILE_EXTENSION = ".duckdb_extension.repo.json";
	//! The version of the repository file format
	static constexpr const idx_t FORMAT_VERSION = 1;
	//! The metadata file that a repository serves under its prefix, describing its trusted signing keys
	static constexpr const char *METADATA_FILE = ".well-known/duckdb-extension-repo.json";

public:
	//! The directory in which the trusted extension repositories are stored
	static string GetRepositoryDirectory(DatabaseInstance &db, FileSystem &fs);

	//! Store a new trusted repository on disk. If the repository has no public keys, they are fetched from the
	//! repository prefix and pinned. `from_registry` says the url was resolved through the extension repository
	//! registry (FROM '<host>'), which the 'undecided' default of allow_extension_repositories permits. Returns the
	//! repository as it was stored
	static ExtensionRepository CreateRepository(DatabaseInstance &db, FileSystem &fs,
	                                            optional_ptr<ClientContext> context,
	                                            const ExtensionRepository &repository, OnCreateConflict on_conflict,
	                                            bool from_registry = false);
	//! Remove a trusted repository from disk
	static void DropRepository(DatabaseInstance &db, FileSystem &fs, const string &name,
	                           OnEntryNotFound on_entry_not_found);

	//! Get all trusted repositories that were added by the user
	static vector<ExtensionRepository> GetRepositories(DatabaseInstance &db, FileSystem &fs);
	//! Try to look up a trusted repository by name
	static bool TryGetRepository(DatabaseInstance &db, FileSystem &fs, const string &name, ExtensionRepository &result);
	//! Try to look up a trusted repository by the host it was resolved from. The name is the user's alias, the host
	//! is the identity, so this is how code refers to a repository. Throws when more than one repository was created
	//! from the host: which one is meant is then the user's call, not a guess
	static bool TryGetRepositoryByHost(DatabaseInstance &db, FileSystem &fs, const string &host,
	                                   ExtensionRepository &result);
	//! The host as it is compared and stored: lower case. Throws if it is not a plain host name
	static string NormalizeHost(const string &host);

	//! Repository names are case insensitive and are used as file names - restrict them to a safe character set
	static string NormalizeRepositoryName(const string &name);
	static bool TryNormalizeRepositoryName(const string &name, string &result);

	//! Whether the user has opted in to adding external extension repositories
	static ExtensionRepositoryAccess GetAccess(DatabaseInstance &db);
	//! Whether any of the repositories that were already created may be trusted: their extensions can be installed and
	//! loaded. Under 'known_hosts' only some are, see TrustsRepository
	static bool TrustsExistingRepositories(DatabaseInstance &db);
	//! Whether this repository is trusted: under 'known_hosts' only a repository with a host is
	static bool TrustsRepository(DatabaseInstance &db, const ExtensionRepository &repository);
	//! Parse the access value of the allow_extension_repositories setting (throws on an invalid value)
	static ExtensionRepositoryAccess ParseAccess(const string &value);
	//! The string representation of an access value, as used by the allow_extension_repositories setting
	static string AccessToString(ExtensionRepositoryAccess access);

	//! Fetch the trusted signing keys from the metadata file that a repository serves under its prefix. The file is a
	//! json object with a "signature_keys" array of PEM-encoded keys
	static vector<string> FetchPublicKeys(DatabaseInstance &db, optional_ptr<ClientContext> context,
	                                      const string &prefix);

	//! Resolve `CREATE EXTENSION REPOSITORY ... FROM '<host>'` to the repository url. The registry entry for the host
	//! (`<extension_repository_registry>/<host>`) names the url; when it also names a verifier path, the consent file
	//! the host serves there must name the same url. `verified_on_host` reports whether that check ran
	static string ResolveRepositoryUrlFromHost(DatabaseInstance &db, optional_ptr<ClientContext> context,
	                                           const string &host, bool &verified_on_host);

	//! The sha256 fingerprint of a public key, used to compare a key against the fingerprint published by a repository
	static string GetPublicKeyFingerprint(const string &compact_key);

	//! Extract all public keys from a blob that may contain one or more keys in PEM or compact form
	static vector<string> ToCompactPublicKeys(const string &public_keys);
	//! Flatten a list of provided key blobs (each of which may contain multiple keys) into compact keys
	static vector<string> NormalizePublicKeys(const vector<string> &public_keys);
	//! Convert a public key in PEM or compact form into the compact single-line representation
	static string ToCompactPublicKey(const string &public_key);
	//! Convert a compact public key back into the PEM representation used for signature verification
	static string ToPEMPublicKey(const string &compact_key);

private:
	//! Throws unless adding a repository is currently allowed (opt-in and configuration lock)
	static void CheckCanCreate(DatabaseInstance &db, bool from_registry);
	//! Throws if the configuration is locked, freezing the trust store
	static void CheckNotLocked(DatabaseInstance &db);
	static string GetRepositoryPath(DatabaseInstance &db, FileSystem &fs, const string &name);
	static ExtensionRepository ReadRepository(FileSystem &fs, const string &path);
};

} // namespace duckdb
