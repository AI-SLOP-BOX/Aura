//! Safe extension discovery for the Aura plug-in ecosystem.
//!
//! Extensions are manifest-only at this boundary.  Discovery never executes
//! code, follows symlinks, or grants filesystem access; a later host can add a
//! sandboxed runtime behind this stable contract.

use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
pub struct ExtensionManifest {
    pub id: String,
    pub name: String,
    pub version: String,
    #[serde(default)]
    pub description: String,
    #[serde(default)]
    pub capabilities: Vec<String>,
    #[serde(default)]
    pub permissions: Vec<String>,
    #[serde(default = "default_execution")]
    pub execution: String,
    /// Declared scripting runtime. The host never executes an undeclared
    /// language, keeping Lua/Python/JavaScript support explicit and auditable.
    #[serde(default = "default_runtime_language")]
    pub runtime_language: String,
    /// Relative executable used only by trusted extensions with the explicit
    /// `process_spawn` permission. Sandboxed extensions never execute code.
    #[serde(default)]
    pub entrypoint: Option<String>,
    #[serde(default)]
    pub commands: Vec<ExtensionCommand>,
    /// Declarative UI contributions.  The host can render these without
    /// loading extension code, which keeps discovery safe while allowing an
    /// extension to add a panel or command-palette entry immediately.
    #[serde(default)]
    pub contributions: ExtensionContributions,
}

#[derive(Debug, Clone, Default, Deserialize, Serialize, PartialEq, Eq)]
pub struct ExtensionContributions {
    #[serde(default)]
    pub panels: Vec<ExtensionPanel>,
    #[serde(default)]
    pub menus: Vec<ExtensionMenuItem>,
}

#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
pub struct ExtensionPanel {
    pub id: String,
    pub title: String,
    #[serde(default)]
    pub location: String,
    #[serde(default)]
    pub icon: String,
}

#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
pub struct ExtensionMenuItem {
    pub id: String,
    pub title: String,
    pub command_id: String,
    #[serde(default)]
    pub menu: String,
}

#[derive(Debug, Clone, Deserialize, Serialize, PartialEq, Eq)]
pub struct ExtensionCommand {
    pub id: String,
    pub title: String,
    #[serde(default = "default_command_kind")]
    pub kind: String,
    #[serde(default = "default_input_schema")]
    pub input_schema: serde_json::Value,
}

fn default_command_kind() -> String {
    "read_only".into()
}

fn default_execution() -> String {
    "sandboxed".into()
}

fn default_runtime_language() -> String { "none".into() }

fn default_input_schema() -> serde_json::Value {
    serde_json::json!({"type": "object", "additionalProperties": false})
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct DiscoveredExtension {
    pub manifest: ExtensionManifest,
    pub root: PathBuf,
    /// Project-local activation state.  Discovery remains manifest-only, but
    /// disabled extensions must not silently reappear in command/UI registries.
    pub enabled: bool,
}

/// Built-in marketplace index metadata. Network fetching is intentionally
/// outside the audio process; clients can use these records to present a
/// compatible, auditable install target before supplying a local package.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct MarketplaceListing {
    pub extension_id: String,
    pub version: String,
    pub channel: String,
    pub package_format: String,
    pub signature_required: bool,
    pub permissions_review_required: bool,
}

pub fn marketplace_catalog() -> Vec<MarketplaceListing> {
    vec![MarketplaceListing {
        extension_id: "aura.example.effects".into(),
        version: "0.1.0".into(),
        channel: "stable".into(),
        package_format: "manifest-directory".into(),
        signature_required: true,
        permissions_review_required: true,
    }]
}

/// Search the local marketplace index without network access or installation.
/// Empty filters match all listings; channel matching is case-insensitive.
pub fn marketplace_search(query: &str, channel: Option<&str>) -> Vec<MarketplaceListing> {
    let needle = query.trim().to_ascii_lowercase();
    let requested_channel = channel.map(|value| value.trim().to_ascii_lowercase());
    marketplace_catalog().into_iter().filter(|listing| {
        let query_matches = needle.is_empty()
            || listing.extension_id.to_ascii_lowercase().contains(&needle)
            || listing.version.to_ascii_lowercase().contains(&needle);
        let channel_matches = requested_channel.as_ref()
            .is_none_or(|value| value == &listing.channel.to_ascii_lowercase());
        query_matches && channel_matches
    }).collect()
}

fn activation_path(root: &Path) -> PathBuf {
    root.join(".aura").join("extensions.json")
}

fn activation_state(root: &Path) -> std::collections::HashMap<String, bool> {
    let path = activation_path(root);
    let Ok(metadata) = fs::symlink_metadata(&path) else { return std::collections::HashMap::new() };
    if metadata.file_type().is_symlink() || !metadata.is_file() { return std::collections::HashMap::new() }
    fs::read_to_string(path)
        .ok()
        .and_then(|contents| serde_json::from_str(&contents).ok())
        .unwrap_or_default()
}

/// Persist activation state without executing or loading extension code.
/// The file is project-local and written with a unique temporary name so two
/// independent UI/CLI clients cannot accidentally truncate one another's
/// settings.
pub fn set_enabled(root: impl AsRef<Path>, extension_id: &str, enabled: bool) -> Result<(), String> {
    if !valid_token(extension_id) { return Err("extension id is invalid".into()); }
    let root = root.as_ref();
    let (discovered, _) = discover(root);
    if !discovered.iter().any(|extension| extension.manifest.id == extension_id) {
        return Err("extension is not installed under this root".into());
    }
    let mut state = activation_state(root);
    state.insert(extension_id.to_owned(), enabled);
    fs::create_dir_all(root.join(".aura")).map_err(|e| e.to_string())?;
    let path = activation_path(root);
    let temp = path.with_extension(format!("tmp-{}-{}", std::process::id(), uuid::Uuid::new_v4()));
    let bytes = serde_json::to_vec_pretty(&state).map_err(|e| e.to_string())?;
    {
        use std::io::Write;
        let mut file = fs::File::create(&temp).map_err(|e| e.to_string())?;
        file.write_all(&bytes).map_err(|e| e.to_string())?;
        file.sync_all().map_err(|e| e.to_string())?;
    }
    fs::rename(&temp, &path).map_err(|e| e.to_string())?;
    #[cfg(unix)]
    fs::File::open(root.join(".aura")).and_then(|file| file.sync_all()).map_err(|e| e.to_string())?;
    Ok(())
}

/// Install a manifest-based extension directory into a project-local root.
/// Only regular files are copied; symlinks and path traversal are rejected.
/// The destination is published with a final rename so discovery never sees a
/// partially copied extension.
pub fn install_from_directory(source: impl AsRef<Path>, root: impl AsRef<Path>) -> Result<String, String> {
    let source = source.as_ref();
    let root = root.as_ref();
    let source_meta = fs::symlink_metadata(source).map_err(|error| format!("invalid extension source: {error}"))?;
    if !source_meta.is_dir() || source_meta.file_type().is_symlink() {
        return Err("extension source must be a regular directory".into());
    }
    let manifest_path = source.join("manifest.json");
    let manifest_meta = fs::symlink_metadata(&manifest_path).map_err(|error| format!("manifest is missing: {error}"))?;
    if !manifest_meta.is_file() || manifest_meta.file_type().is_symlink() {
        return Err("extension manifest must be a regular file".into());
    }
    let manifest: ExtensionManifest = serde_json::from_str(&fs::read_to_string(&manifest_path).map_err(|error| error.to_string())?)
        .map_err(|error| format!("invalid extension manifest: {error}"))?;
    validate_manifest(&manifest)?;
    fs::create_dir_all(root).map_err(|error| error.to_string())?;
    let destination = root.join(&manifest.id);
    if destination.exists() {
        return Err("an extension with this id is already installed".into());
    }
    let temporary = root.join(format!(".{}.install-{}-{}", manifest.id, std::process::id(), uuid::Uuid::new_v4()));
    let mut budget = CopyBudget { files: 0, bytes: 0 };
    if let Err(error) = copy_extension_tree(source, &temporary, &mut budget) {
        let _ = fs::remove_dir_all(&temporary);
        return Err(error);
    }
    if let Err(error) = fs::rename(&temporary, &destination) {
        let _ = fs::remove_dir_all(&temporary);
        return Err(format!("cannot publish extension: {error}"));
    }
    #[cfg(unix)]
    fs::File::open(root).and_then(|file| file.sync_all()).map_err(|error| error.to_string())?;
    Ok(manifest.id)
}

struct CopyBudget { files: u64, bytes: u64 }

fn copy_extension_tree(source: &Path, destination: &Path, budget: &mut CopyBudget) -> Result<(), String> {
    const MAX_FILES: u64 = 4096;
    const MAX_BYTES: u64 = 256 * 1024 * 1024;
    fs::create_dir_all(destination).map_err(|error| error.to_string())?;
    for entry in fs::read_dir(source).map_err(|error| error.to_string())? {
        let entry = entry.map_err(|error| error.to_string())?;
        let file_type = entry.file_type().map_err(|error| error.to_string())?;
        if file_type.is_symlink() {
            return Err(format!("extension contains a symlink: {}", entry.path().display()));
        }
        let target = destination.join(entry.file_name());
        if file_type.is_dir() {
            copy_extension_tree(&entry.path(), &target, budget)?;
        } else if file_type.is_file() {
            budget.files = budget.files.saturating_add(1);
            let size = entry.metadata().map_err(|error| error.to_string())?.len();
            budget.bytes = budget.bytes.saturating_add(size);
            if budget.files > MAX_FILES || budget.bytes > MAX_BYTES {
                return Err("extension exceeds the 4096-file or 256 MiB install limit".into());
            }
            fs::copy(entry.path(), target).map_err(|error| error.to_string())?;
        } else {
            return Err(format!("extension contains unsupported file: {}", entry.path().display()));
        }
    }
    Ok(())
}

/// Return the effective activation state for an installed extension.  The
/// distinction between `None` (not installed) and `Some(false)` (installed
/// but disabled) is needed by transaction rollback.
pub fn enabled_state(root: impl AsRef<Path>, extension_id: &str) -> Option<bool> {
    discover(root)
        .0
        .into_iter()
        .find(|extension| extension.manifest.id == extension_id)
        .map(|extension| extension.enabled)
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct ExtensionCommandRegistration {
    pub extension_id: String,
    pub command_id: String,
    pub qualified_id: String,
    pub title: String,
    pub kind: String,
    pub execution: String,
    pub permissions: Vec<String>,
    pub input_schema: serde_json::Value,
    pub root: PathBuf,
    pub entrypoint: Option<String>,
    pub enabled: bool,
}

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct ExtensionUiRegistration {
    pub extension_id: String,
    pub kind: String,
    pub id: String,
    pub title: String,
    pub location: String,
    pub command_id: Option<String>,
    pub icon: Option<String>,
}

struct ExtensionRunAudit {
    path: PathBuf,
    extension_id: String,
    command_id: String,
    run_id: String,
    payload_hash: String,
    started_unix_seconds: u64,
    status: String,
}

impl ExtensionRunAudit {
    fn new(root: &Path, extension_id: &str, command_id: &str, run_id: &str, payload: &serde_json::Value) -> Self {
        Self {
            path: root.join(".aura").join("extension-runs.jsonl"),
            extension_id: extension_id.to_owned(), command_id: command_id.to_owned(), run_id: run_id.to_owned(),
            payload_hash: hash_json(payload),
            started_unix_seconds: SystemTime::now().duration_since(UNIX_EPOCH).map_or(0, |value| value.as_secs()),
            status: "aborted".to_owned(),
        }
    }

    fn finish(&mut self, status: &str) { self.status = status.to_owned(); }
}

impl Drop for ExtensionRunAudit {
    fn drop(&mut self) {
        let Ok(parent) = self.path.parent().map(Path::to_path_buf).ok_or(()) else { return; };
        if fs::create_dir_all(parent).is_err() { return; }
        let record = serde_json::json!({
            "run_id": self.run_id, "extension_id": self.extension_id, "command_id": self.command_id,
            "payload_sha256": self.payload_hash, "started_unix_seconds": self.started_unix_seconds,
            "status": self.status,
        });
        let Ok(mut file) = fs::OpenOptions::new().create(true).append(true).open(&self.path) else { return; };
        let _ = writeln!(file, "{}", record);
        let _ = file.sync_all();
    }
}

fn hash_json(value: &serde_json::Value) -> String {
    let bytes = serde_json::to_vec(value).unwrap_or_default();
    let mut hasher = Sha256::new();
    hasher.update(bytes);
    hasher.finalize().iter().map(|byte| format!("{byte:02x}")).collect()
}

/// Read only the bounded recent audit trail. Invalid or oversized records are
/// ignored so a damaged log can never prevent extension discovery.
pub fn recent_run_history(root: impl AsRef<Path>) -> Vec<serde_json::Value> {
    let path = root.as_ref().join(".aura").join("extension-runs.jsonl");
    let Ok(contents) = fs::read_to_string(path) else { return Vec::new(); };
    contents.lines().rev().take(64).filter_map(|line| {
        let value = serde_json::from_str::<serde_json::Value>(line).ok()?;
        value.is_object().then_some(value)
    }).collect()
}

fn valid_token(value: &str) -> bool {
    !value.is_empty()
        && value.len() <= 128
        && value == value.trim()
        && value
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || b"._-".contains(&byte))
}

fn validate_manifest(manifest: &ExtensionManifest) -> Result<(), String> {
    if !valid_token(&manifest.id) {
        return Err("extension id is invalid".into());
    }
    if manifest.name.trim().is_empty() || manifest.name.len() > 256 {
        return Err("extension name is invalid".into());
    }
    if !matches!(manifest.execution.as_str(), "sandboxed" | "trusted") {
        return Err("extension execution mode is invalid".into());
    }
    if !matches!(manifest.runtime_language.as_str(), "none" | "lua" | "python" | "javascript") {
        return Err("extension runtime language is unsupported".into());
    }
    if manifest.runtime_language != "none" && manifest.entrypoint.is_none() {
        return Err("scripted extension requires an entrypoint".into());
    }
    if let Some(entrypoint) = &manifest.entrypoint {
        let path = Path::new(entrypoint);
        if manifest.execution != "trusted"
            || !manifest.permissions.iter().any(|permission| permission == "process_spawn")
            || entrypoint.len() > 512
            || path.is_absolute()
            || path.components().any(|component| matches!(component, std::path::Component::ParentDir | std::path::Component::RootDir | std::path::Component::Prefix(_)))
        {
            return Err("extension entrypoint requires trusted process_spawn permission and a safe relative path".into());
        }
    }
    if !valid_token(&manifest.version) {
        return Err("extension version is invalid".into());
    }
    if manifest.permissions.iter().any(|permission| {
        !matches!(
            permission.as_str(),
            "project_read"
                | "project_write"
                | "audio_process"
                | "filesystem_external"
                | "network"
                | "process_spawn"
        )
    }) {
        return Err("extension requests an unsupported permission".into());
    }
    if manifest.execution == "sandboxed"
        && manifest
            .permissions
            .iter()
            .any(|permission| matches!(permission.as_str(), "filesystem_external" | "network" | "process_spawn"))
    {
        return Err("sandboxed extensions cannot request external side effects".into());
    }
    if manifest.commands.iter().any(|command| {
        !valid_token(&command.id)
            || command.title.trim().is_empty()
            || !matches!(command.kind.as_str(), "read_only" | "reversible" | "external_side_effect")
            || !command.input_schema.is_object()
    }) {
        return Err("extension declares an invalid command".into());
    }
    if manifest.contributions.panels.len() > 64 || manifest.contributions.menus.len() > 256 {
        return Err("extension declares too many UI contributions".into());
    }
    if manifest.contributions.panels.iter().any(|panel| {
        !valid_token(&panel.id)
            || panel.title.trim().is_empty()
            || panel.title.len() > 256
            || (!panel.location.is_empty()
                && !matches!(panel.location.as_str(), "left" | "right" | "bottom" | "floating"))
    }) {
        return Err("extension declares an invalid panel contribution".into());
    }
    if manifest.contributions.menus.iter().any(|item| {
        !valid_token(&item.id)
            || item.title.trim().is_empty()
            || item.title.len() > 256
            || !valid_token(&item.command_id)
            || (!item.menu.is_empty()
                && !matches!(item.menu.as_str(), "file" | "edit" | "view" | "track" | "plugin" | "help"))
    }) {
        return Err("extension declares an invalid menu contribution".into());
    }
    if manifest.execution == "sandboxed"
        && manifest.commands.iter().any(|command| command.kind == "external_side_effect")
    {
        return Err("sandboxed extensions cannot declare external-side-effect commands".into());
    }
    Ok(())
}

/// Discover manifest-only extensions beneath an explicit project-owned root.
/// Invalid entries are ignored and returned in `errors`; valid entries are
/// sorted by id for deterministic CLI/UI output.
pub fn discover(root: impl AsRef<Path>) -> (Vec<DiscoveredExtension>, Vec<String>) {
    let root = root.as_ref();
    let activation = activation_state(root);
    let mut found = Vec::new();
    let mut errors = Vec::new();
    let Ok(entries) = fs::read_dir(root) else { return (found, errors) };
    for entry in entries.flatten() {
        let path = entry.path();
        let Ok(file_type) = entry.file_type() else { continue };
        if !file_type.is_dir() || file_type.is_symlink() {
            continue;
        }
        let manifest_path = path.join("manifest.json");
        let Ok(manifest_type) = fs::symlink_metadata(&manifest_path) else { continue };
        if !manifest_type.is_file() || manifest_type.file_type().is_symlink() {
            errors.push(format!("{}: manifest must be a regular file", path.display()));
            continue;
        }
        let result = fs::read_to_string(&manifest_path)
            .map_err(|error| error.to_string())
            .and_then(|contents| serde_json::from_str::<ExtensionManifest>(&contents).map_err(|error| error.to_string()))
            .and_then(|manifest| validate_manifest(&manifest).map(|()| manifest));
        match result {
            Ok(manifest) => {
                let enabled = activation.get(&manifest.id).copied().unwrap_or(true);
                found.push(DiscoveredExtension { manifest, root: path, enabled });
            }
            Err(error) => errors.push(format!("{}: {error}", manifest_path.display())),
        }
    }
    found.sort_by(|left, right| left.manifest.id.cmp(&right.manifest.id));
    (found, errors)
}

/// Turn discovered manifests into a deterministic command registry without
/// executing extension code. Qualified IDs prevent two extensions from
/// claiming the same command, while reserved core names remain protected.
pub fn command_registry(root: impl AsRef<Path>) -> (Vec<ExtensionCommandRegistration>, Vec<String>) {
    let (extensions, mut errors) = discover(root);
    let reserved = ["project.inspect", "control.inspect", "plugin_catalog", "history.commit"];
    let mut ids = std::collections::HashSet::new();
    let mut registrations = Vec::new();
    for extension in extensions {
        if !extension.enabled { continue; }
        for command in extension.manifest.commands {
            let qualified_id = format!("{}.{}", extension.manifest.id, command.id);
            if reserved.contains(&qualified_id.as_str()) || !ids.insert(qualified_id.clone()) {
                errors.push(format!("duplicate or reserved extension command: {qualified_id}"));
                continue;
            }
            registrations.push(ExtensionCommandRegistration {
                extension_id: extension.manifest.id.clone(),
                command_id: command.id,
                qualified_id,
                title: command.title,
                kind: command.kind,
                execution: extension.manifest.execution.clone(),
                permissions: extension.manifest.permissions.clone(),
                input_schema: command.input_schema,
                root: extension.root.clone(),
                entrypoint: extension.manifest.entrypoint.clone(),
                enabled: true,
            });
        }
    }
    registrations.sort_by(|left, right| left.qualified_id.cmp(&right.qualified_id));
    (registrations, errors)
}

/// Complete installed-command catalog for UI management. Disabled commands
/// remain visible here so users can re-enable them; execution still uses the
/// filtered command registry above.
pub fn command_catalog(root: impl AsRef<Path>) -> (Vec<ExtensionCommandRegistration>, Vec<String>) {
    let (extensions, mut errors) = discover(root);
    let mut ids = std::collections::HashSet::new();
    let mut registrations = Vec::new();
    for extension in extensions {
        for command in extension.manifest.commands {
            let qualified_id = format!("{}.{}", extension.manifest.id, command.id);
            if !ids.insert(qualified_id.clone()) {
                errors.push(format!("duplicate extension command: {qualified_id}"));
                continue;
            }
            registrations.push(ExtensionCommandRegistration {
                extension_id: extension.manifest.id.clone(), command_id: command.id,
                qualified_id, title: command.title, kind: command.kind,
                execution: extension.manifest.execution.clone(), permissions: extension.manifest.permissions.clone(),
                input_schema: command.input_schema, root: extension.root.clone(),
                entrypoint: extension.manifest.entrypoint.clone(), enabled: extension.enabled,
            });
        }
    }
    registrations.sort_by(|left, right| left.qualified_id.cmp(&right.qualified_id));
    (registrations, errors)
}

/// Execute one trusted extension command through the small Aura process
/// protocol. The executable receives a JSON object on stdin and must return a
/// JSON object on stdout. The runner is deliberately project-local, bounded,
/// and never available to sandboxed manifests.
pub fn invoke_command(
    root: impl AsRef<Path>,
    extension_id: &str,
    command_id: &str,
    payload: &serde_json::Value,
    timeout_ms: u64,
) -> Result<serde_json::Value, String> {
    if !valid_token(extension_id) || !valid_token(command_id) {
        return Err("extension command identity is invalid".into());
    }
    if !(1..=30_000).contains(&timeout_ms) {
        return Err("extension timeout must be within 1..=30000ms".into());
    }
    let (extensions, errors) = discover(root.as_ref());
    if !errors.is_empty() {
        return Err(format!("extension discovery failed: {}", errors.join("; ")));
    }
    let extension = extensions
        .into_iter()
        .find(|item| item.manifest.id == extension_id && item.enabled)
        .ok_or_else(|| "enabled extension is not installed under this root".to_owned())?;
    if extension.manifest.execution != "trusted"
        || !extension.manifest.permissions.iter().any(|permission| permission == "process_spawn")
    {
        return Err("extension execution requires trusted mode and process_spawn permission".into());
    }
    let command = extension
        .manifest
        .commands
        .iter()
        .find(|item| item.id == command_id)
        .ok_or_else(|| "extension command is not declared".to_owned())?;
    let registration = ExtensionCommandRegistration {
        extension_id: extension.manifest.id.clone(),
        command_id: command.id.clone(),
        qualified_id: format!("{extension_id}.{command_id}"),
        title: command.title.clone(),
        kind: command.kind.clone(),
        execution: extension.manifest.execution.clone(),
        permissions: extension.manifest.permissions.clone(),
        input_schema: command.input_schema.clone(),
        root: extension.root.clone(),
        entrypoint: extension.manifest.entrypoint.clone(),
        enabled: true,
    };
    validate_command_payload(&registration, payload)?;
    let entrypoint = registration.entrypoint.as_deref().ok_or_else(|| "trusted extension has no entrypoint".to_owned())?;
    let entrypoint_path = validate_entrypoint(&extension.root, entrypoint)?;

    let run_dir = root.as_ref().join(".aura").join("extension-runs");
    fs::create_dir_all(&run_dir).map_err(|error| format!("cannot create extension run directory: {error}"))?;
    let run_id = uuid::Uuid::new_v4().to_string();
    let mut audit = ExtensionRunAudit::new(root.as_ref(), extension_id, command_id, &run_id, payload);
    let input_path = run_dir.join(format!("{run_id}.input"));
    let output_path = run_dir.join(format!("{run_id}.output"));
    let request = serde_json::json!({
        "protocol": "aura.extension.v1",
        "extension_id": extension_id,
        "command_id": command_id,
        "payload": payload,
    });
    {
        let mut file = fs::File::create(&input_path).map_err(|error| error.to_string())?;
        let bytes = serde_json::to_vec(&request).map_err(|error| error.to_string())?;
        file.write_all(&bytes).map_err(|error| error.to_string())?;
        file.sync_all().map_err(|error| error.to_string())?;
    }
    let mut child = Command::new(&entrypoint_path)
        .current_dir(&extension.root)
        .stdin(fs::File::open(&input_path).map_err(|error| error.to_string())?)
        .stdout(fs::File::create(&output_path).map_err(|error| error.to_string())?)
        .stderr(Stdio::null())
        .env_clear()
        .env("AURA_EXTENSION_PROTOCOL", "aura.extension.v1")
        .spawn()
        .map_err(|error| error.to_string())?;
    let deadline = Instant::now() + Duration::from_millis(timeout_ms);
    loop {
        if let Some(status) = child.try_wait().map_err(|error| error.to_string())? {
            let _ = fs::remove_file(&input_path);
            if !status.success() {
                let _ = fs::remove_file(&output_path);
                audit.finish("failed");
                return Err(format!("extension exited with status {status}"));
            }
            break;
        }
        if Instant::now() >= deadline {
            let _ = child.kill();
            let _ = child.wait();
            let _ = fs::remove_file(&input_path);
            let _ = fs::remove_file(&output_path);
            audit.finish("timed_out");
            return Err("extension command timed out".into());
        }
        std::thread::sleep(Duration::from_millis(2));
    }
    let metadata = fs::metadata(&output_path).map_err(|error| error.to_string())?;
    if metadata.len() > 1_048_576 {
        let _ = fs::remove_file(&output_path);
        audit.finish("failed_output_limit");
        return Err("extension output exceeds the 1 MiB limit".into());
    }
    let output = fs::read_to_string(&output_path).map_err(|error| error.to_string())?;
    let _ = fs::remove_file(&output_path);
    let value: serde_json::Value = serde_json::from_str(&output).map_err(|error| {
        audit.finish("failed_invalid_json");
        format!("extension returned invalid JSON: {error}")
    })?;
    if !value.is_object() {
        audit.finish("failed_invalid_result");
        return Err("extension result must be a JSON object".into());
    }
    audit.finish("completed");
    Ok(value)
}

fn validate_entrypoint(root: &Path, entrypoint: &str) -> Result<PathBuf, String> {
    let relative = Path::new(entrypoint);
    if entrypoint.is_empty() || relative.is_absolute() || relative.components().any(|component| matches!(component, std::path::Component::ParentDir | std::path::Component::RootDir | std::path::Component::Prefix(_))) {
        return Err("extension entrypoint must be a relative path without traversal".into());
    }
    let path = root.join(relative);
    let metadata = fs::symlink_metadata(&path).map_err(|error| format!("invalid extension entrypoint: {error}"))?;
    if metadata.file_type().is_symlink() || !metadata.is_file() {
        return Err("extension entrypoint must be a regular non-symlink file".into());
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        if metadata.permissions().mode() & 0o111 == 0 {
            return Err("extension entrypoint is not executable".into());
        }
    }
    Ok(path)
}

/// Build a deterministic declarative UI registry.  IDs are qualified exactly
/// like commands, and every menu contribution must point at a command owned by
/// the same extension.  No extension code is executed here.
pub fn ui_registry(root: impl AsRef<Path>) -> (Vec<ExtensionUiRegistration>, Vec<String>) {
    let (extensions, mut errors) = discover(root);
    let mut registrations = Vec::new();
    for extension in extensions {
        if !extension.enabled { continue; }
        let command_ids: std::collections::HashSet<_> = extension
            .manifest
            .commands
            .iter()
            .map(|command| command.id.as_str())
            .collect();
        for panel in extension.manifest.contributions.panels {
            registrations.push(ExtensionUiRegistration {
                extension_id: extension.manifest.id.clone(),
                kind: "panel".to_owned(),
                id: format!("{}.{}", extension.manifest.id, panel.id),
                title: panel.title,
                location: if panel.location.is_empty() { "right".to_owned() } else { panel.location },
                command_id: None,
                icon: (!panel.icon.is_empty()).then_some(panel.icon),
            });
        }
        for menu in extension.manifest.contributions.menus {
            if !command_ids.contains(menu.command_id.as_str()) {
                errors.push(format!("menu contribution references unknown command: {}.{}", extension.manifest.id, menu.command_id));
                continue;
            }
            registrations.push(ExtensionUiRegistration {
                extension_id: extension.manifest.id.clone(),
                kind: "menu".to_owned(),
                id: format!("{}.{}", extension.manifest.id, menu.id),
                title: menu.title,
                location: if menu.menu.is_empty() { "view".to_owned() } else { menu.menu },
                command_id: Some(format!("{}.{}", extension.manifest.id, menu.command_id)),
                icon: None,
            });
        }
    }
    registrations.sort_by(|left, right| left.id.cmp(&right.id));
    (registrations, errors)
}

/// Validate an extension payload against its declared, intentionally small
/// JSON-schema subset before any command is admitted to the host boundary.
pub fn validate_command_payload(
    command: &ExtensionCommandRegistration,
    payload: &serde_json::Value,
) -> Result<(), String> {
    const MAX_PAYLOAD_BYTES: usize = 1_048_576;
    if serde_json::to_vec(payload).map_err(|_| "extension payload is not serializable")?.len()
        > MAX_PAYLOAD_BYTES
    {
        return Err("extension payload exceeds the 1 MiB limit".into());
    }
    let Some(object) = payload.as_object() else {
        return Err("extension payload must be a JSON object".into());
    };
    let schema = &command.input_schema;
    if schema.get("type").and_then(serde_json::Value::as_str) != Some("object") {
        return Err("extension input schema must describe an object".into());
    }
    if schema
        .get("additionalProperties")
        .and_then(serde_json::Value::as_bool)
        == Some(false)
    {
        let properties = schema.get("properties").and_then(serde_json::Value::as_object);
        if object
            .keys()
            .any(|key| properties.is_none_or(|fields| !fields.contains_key(key)))
        {
            return Err("extension payload contains an unknown field".into());
        }
    }
    if let Some(required) = schema.get("required").and_then(serde_json::Value::as_array) {
        for field in required.iter().filter_map(serde_json::Value::as_str) {
            if !object.contains_key(field) {
                return Err(format!("extension payload is missing required field {field}"));
            }
        }
    }
    if let Some(properties) = schema.get("properties").and_then(serde_json::Value::as_object) {
        for (name, definition) in properties {
            let Some(value) = object.get(name) else { continue };
            let Some(expected) = definition.get("type").and_then(serde_json::Value::as_str) else { continue };
            let valid = match expected {
                "string" => value.is_string(),
                "number" => value.as_f64().is_some_and(f64::is_finite),
                "integer" => value.as_i64().is_some(),
                "boolean" => value.is_boolean(),
                "array" => value.is_array(),
                "object" => value.is_object(),
                "null" => value.is_null(),
                _ => false,
            };
            if !valid {
                return Err(format!("extension field {name} has the wrong type"));
            }
            if let Some(values) = definition.get("enum").and_then(serde_json::Value::as_array) {
                if !values.iter().any(|candidate| candidate == value) {
                    return Err(format!("extension field {name} is not an allowed value"));
                }
            }
            if let Some(length) = value.as_str().map(str::len) {
                if definition.get("minLength").and_then(serde_json::Value::as_u64)
                    .is_some_and(|minimum| (length as u64) < minimum)
                    || definition.get("maxLength").and_then(serde_json::Value::as_u64)
                        .is_some_and(|maximum| (length as u64) > maximum)
                {
                    return Err(format!("extension field {name} has an invalid string length"));
                }
            }
            if let Some(length) = value.as_array().map(Vec::len) {
                if definition.get("minItems").and_then(serde_json::Value::as_u64)
                    .is_some_and(|minimum| (length as u64) < minimum)
                    || definition.get("maxItems").and_then(serde_json::Value::as_u64)
                        .is_some_and(|maximum| (length as u64) > maximum)
                {
                    return Err(format!("extension field {name} has an invalid array length"));
                }
            }
            if let Some(number) = value.as_f64() {
                if !number.is_finite()
                    || definition.get("minimum").and_then(serde_json::Value::as_f64)
                        .is_some_and(|minimum| number < minimum)
                    || definition.get("maximum").and_then(serde_json::Value::as_f64)
                        .is_some_and(|maximum| number > maximum)
                {
                    return Err(format!("extension field {name} is outside its allowed range"));
                }
            }
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn discovers_sorted_manifest_only_extensions() {
        let root = std::env::temp_dir().join(format!("aura-extensions-{}", uuid::Uuid::new_v4()));
        fs::create_dir_all(root.join("z-last")).unwrap();
        fs::create_dir_all(root.join("a-first")).unwrap();
        fs::write(root.join("z-last/manifest.json"), r#"{"id":"z-last","name":"Z","version":"1"}"#).unwrap();
        fs::write(root.join("a-first/manifest.json"), r#"{"id":"a-first","name":"A","version":"1"}"#).unwrap();
        let (found, errors) = discover(&root);
        assert!(errors.is_empty());
        assert_eq!(found.iter().map(|item| item.manifest.id.as_str()).collect::<Vec<_>>(), ["a-first", "z-last"]);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn extension_activation_is_project_local_and_registry_filtered() {
        let root = std::env::temp_dir().join(format!("aura-extensions-state-{}", uuid::Uuid::new_v4()));
        fs::create_dir_all(root.join("panel")).unwrap();
        fs::write(root.join("panel/manifest.json"), r#"{"id":"panel","name":"Panel","version":"1","commands":[{"id":"open","title":"Open"}]}"#).unwrap();
        assert!(command_registry(&root).0.iter().any(|command| command.extension_id == "panel"));
        set_enabled(&root, "panel", false).unwrap();
        let (commands, errors) = command_registry(&root);
        assert!(errors.is_empty());
        assert!(commands.is_empty());
        set_enabled(&root, "panel", true).unwrap();
        assert_eq!(command_registry(&root).0.len(), 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn rejects_unsupported_permissions_without_executing_extension() {
        let root = std::env::temp_dir().join(format!("aura-extensions-invalid-{}", uuid::Uuid::new_v4()));
        fs::create_dir_all(root.join("bad")).unwrap();
        fs::write(root.join("bad/manifest.json"), r#"{"id":"bad","name":"Bad","version":"1","permissions":["network"]}"#).unwrap();
        let (found, errors) = discover(&root);
        assert!(found.is_empty());
        assert_eq!(errors.len(), 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn trusted_extensions_can_declare_powerful_permissions_but_are_not_auto_run() {
        let root = std::env::temp_dir().join(format!("aura-extensions-trusted-{}", uuid::Uuid::new_v4()));
        fs::create_dir_all(root.join("trusted")).unwrap();
        fs::write(
            root.join("trusted/manifest.json"),
            r#"{"id":"trusted","name":"Trusted","version":"1","execution":"trusted","permissions":["filesystem_external","network","process_spawn"]}"#,
        ).unwrap();
        let (found, errors) = discover(&root);
        assert!(errors.is_empty());
        assert_eq!(found[0].manifest.execution, "trusted");
        assert!(!found[0].manifest.permissions.is_empty());
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn sandboxed_extensions_cannot_escalate_through_manifest() {
        let root = std::env::temp_dir().join(format!("aura-extensions-escalation-{}", uuid::Uuid::new_v4()));
        fs::create_dir_all(root.join("bad")).unwrap();
        fs::write(
            root.join("bad/manifest.json"),
            r#"{"id":"bad","name":"Bad","version":"1","permissions":["network"]}"#,
        ).unwrap();
        let (found, errors) = discover(&root);
        assert!(found.is_empty());
        assert_eq!(errors.len(), 1);
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn command_registry_qualifies_and_rejects_duplicates() {
        let root = std::env::temp_dir().join(format!("aura-extension-registry-{}", uuid::Uuid::new_v4()));
        fs::create_dir_all(root.join("one")).unwrap();
        fs::create_dir_all(root.join("two")).unwrap();
        let manifest = r#"{"id":"one","name":"One","version":"1","commands":[{"id":"render","title":"Render"}]}"#;
        fs::write(root.join("one/manifest.json"), manifest).unwrap();
        fs::write(root.join("two/manifest.json"), r#"{"id":"two","name":"Two","version":"1","commands":[{"id":"render","title":"Render"}]}"#).unwrap();
        let (commands, errors) = command_registry(&root);
        assert_eq!(commands.iter().map(|item| item.qualified_id.as_str()).collect::<Vec<_>>(), ["one.render", "two.render"]);
        assert!(errors.is_empty());
        let _ = fs::remove_dir_all(root);
    }

    #[cfg(unix)]
    #[test]
    fn trusted_entrypoint_invocation_is_bounded_and_json_based() {
        use std::os::unix::fs::PermissionsExt;
        let root = std::env::temp_dir().join(format!("aura-extension-invoke-{}", uuid::Uuid::new_v4()));
        let extension = root.join("echoer");
        fs::create_dir_all(&extension).unwrap();
        fs::write(
            extension.join("manifest.json"),
            r#"{"id":"echoer","name":"Echoer","version":"1","execution":"trusted","entrypoint":"run.sh","permissions":["process_spawn"],"commands":[{"id":"echo","title":"Echo","input_schema":{"type":"object","additionalProperties":false,"properties":{"value":{"type":"string"}},"required":["value"]}}]}"#,
        ).unwrap();
        fs::write(
            extension.join("run.sh"),
            "#!/bin/sh\ncat >/dev/null\nprintf '%s\n' '{\"ok\":true,\"received\":true}'\n",
        ).unwrap();
        let mut permissions = fs::metadata(extension.join("run.sh")).unwrap().permissions();
        permissions.set_mode(0o700);
        fs::set_permissions(extension.join("run.sh"), permissions).unwrap();
        set_enabled(&root, "echoer", true).unwrap();
        let result = invoke_command(&root, "echoer", "echo", &serde_json::json!({"value":"ok"}), 1_000).unwrap();
        assert_eq!(result["received"], true);
        let audit = fs::read_to_string(root.join(".aura/extension-runs.jsonl")).unwrap();
        assert!(audit.contains("\"status\":\"completed\""));
        assert!(audit.contains("\"payload_sha256\":"));
        assert!(!audit.contains("\"value\":\"ok\""));
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn entrypoint_cannot_be_declared_by_sandboxed_extension() {
        let manifest = ExtensionManifest {
            id: "unsafe".into(), name: "Unsafe".into(), version: "1".into(),
            description: String::new(), capabilities: Vec::new(), permissions: Vec::new(),
            execution: "sandboxed".into(), entrypoint: Some("run.sh".into()),
            runtime_language: "none".into(),
            commands: Vec::new(), contributions: ExtensionContributions::default(),
        };
        assert!(validate_manifest(&manifest).is_err());
    }

    #[test]
    fn installs_valid_extension_atomically_and_rejects_symlinks() {
        let root = std::env::temp_dir().join(format!("aura-extension-install-{}", uuid::Uuid::new_v4()));
        let source = root.join("source");
        let destination = root.join("installed");
        fs::create_dir_all(source.join("assets")).unwrap();
        fs::write(source.join("manifest.json"), r#"{"id":"demo","name":"Demo","version":"1"}"#).unwrap();
        fs::write(source.join("assets/preset.json"), b"{}\n").unwrap();
        assert_eq!(install_from_directory(&source, &destination).unwrap(), "demo");
        assert!(destination.join("demo/manifest.json").is_file());
        assert!(destination.join("demo/assets/preset.json").is_file());
        assert!(install_from_directory(&source, &destination).is_err());
        let _ = fs::remove_dir_all(root);
    }

    #[test]
    fn marketplace_catalog_requires_signature_and_permission_review() {
        let listings = marketplace_catalog();
        assert!(!listings.is_empty());
        assert!(listings.iter().all(|listing| listing.signature_required
            && listing.permissions_review_required
            && listing.channel == "stable"));
    }

    #[test]
    fn marketplace_search_filters_query_and_channel() {
        assert_eq!(marketplace_search("EXAMPLE", Some("STABLE")).len(), 1);
        assert!(marketplace_search("missing", None).is_empty());
        assert_eq!(marketplace_search("", Some("beta")).len(), 0);
    }

    #[cfg(unix)]
    #[test]
    fn extension_install_rejects_symlink_entries_without_partial_publish() {
        use std::os::unix::fs::symlink;
        let root = std::env::temp_dir().join(format!("aura-extension-symlink-{}", uuid::Uuid::new_v4()));
        let source = root.join("source");
        let destination = root.join("installed");
        fs::create_dir_all(&source).unwrap();
        fs::write(source.join("manifest.json"), r#"{"id":"unsafe","name":"Unsafe","version":"1"}"#).unwrap();
        symlink("manifest.json", source.join("link")).unwrap();
        assert!(install_from_directory(&source, &destination).is_err());
        assert!(!destination.join("unsafe").exists());
        let _ = fs::remove_dir_all(root);
    }
}
