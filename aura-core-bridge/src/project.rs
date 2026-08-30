use crate::persistence::{ProjectMetadata, SovereignPersistence};
use crate::project_contracts::{
    validate_contracts, AutomationPointContract, MacroMappingContract, MidiLearnMappingContract, MidiNoteContract, OpenUtauVocalContract, PluginFormat, TempoEventContract,
    PluginInstanceContract, RenderTargetContract, WarpMarkerContract, CompSegmentContract,
    CompTakeContract, TrackStackContract, MarkerContract, PROJECT_CONTRACT_VERSION,
    FreezeArtifactContract, TimeSignatureEventContract, SidechainRouteContract,
    FeedbackRouteContract, AudioRouteContract, VcaGroupContract,
};
use crate::midi::{MIDIEvent, MIDIEventKind};
use crate::hardware_insert::HardwareInsert;
use crate::harmonic::ChordEvent;
use anyhow::{bail, Context, Result};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::collections::HashSet;
use uuid::Uuid;

pub const PROJECT_SCHEMA_VERSION: u32 = 1;
pub const PLUGIN_STATE_SCHEMA_VERSION: u32 = 1;

/// Return stable JSON pointer paths whose values differ between revisions.
pub fn diff_project_json(previous: &serde_json::Value, current: &serde_json::Value) -> Vec<String> {
    fn walk(a: &serde_json::Value, b: &serde_json::Value, path: &str, out: &mut Vec<String>) {
        match (a, b) {
            (serde_json::Value::Object(left), serde_json::Value::Object(right)) => {
                let mut keys: Vec<&String> = left.keys().chain(right.keys()).collect(); keys.sort(); keys.dedup();
                for key in keys { let child = format!("{}/{}", path, key.replace('~', "~0").replace('/', "~1")); match (left.get(key), right.get(key)) { (Some(x), Some(y)) => walk(x,y,&child,out), _ => out.push(child) } }
            }
            (serde_json::Value::Array(left), serde_json::Value::Array(right)) => { if left.len() != right.len() { out.push(format!("{}/length", path)); } for i in 0..left.len().min(right.len()) { walk(&left[i], &right[i], &format!("{}/{}", path, i), out); } }
            _ if a != b => out.push(if path.is_empty() { "/".into() } else { path.into() }),
            _ => {}
        }
    }
    let mut out = Vec::new(); walk(previous, current, "", &mut out); out
}

// These are aggregate admission limits, not performance tuning knobs.  They
// keep a malformed or accidentally enormous project from forcing the native
// hydration path to allocate unbounded memory before it can reject it.
pub const MAX_PROJECT_TRACKS: usize = 4_096;
pub const MAX_PROJECT_REGIONS: usize = 262_144;
pub const MAX_PROJECT_PLUGIN_INSTANCES: usize = 16_384;
pub const MAX_PROJECT_STATE_BYTES: usize = 512 * 1024 * 1024;
pub const MAX_PLUGIN_PARAMETERS: usize = 65_536;
pub const MAX_PROJECT_STRING_BYTES: usize = 64 * 1024 * 1024;
pub const MAX_PROJECT_MIDI_BYTES: usize = 64 * 1024 * 1024;
pub const MAX_PROJECT_COMP_TAKES: usize = 16_384;
pub const MAX_PROJECT_COMP_SEGMENTS: usize = 262_144;
pub const MAX_FREEZE_SAMPLES: u64 = 64 * 1024 * 1024;
pub const MAX_FREEZE_CACHE_BYTES: u64 = 512 * 1024 * 1024;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ProjectDocument {
    pub schema_version: u32,
    #[serde(default = "default_contract_version")]
    pub contract_version: u32,
    /// Stable identity independent of the project's filesystem path.
    #[serde(default = "new_project_id")]
    pub project_id: String,
    pub metadata: ProjectMetadata,
    pub sample_rate: f64,
    #[serde(default = "default_master_gain")]
    pub master_gain: f32,
    #[serde(default)]
    pub cycle_start_sample: u64,
    #[serde(default)]
    pub cycle_end_sample: u64,
    #[serde(default)]
    pub cycle_enabled: bool,
    #[serde(default)]
    pub metronome_enabled: bool,
    pub tracks: Vec<ProjectTrack>,
    /// Track IDs whose signal path is a Bus but whose user-facing role is Aux.
    #[serde(default)]
    pub aux_track_ids: Vec<u32>,
    pub regions: Vec<ProjectRegion>,
    #[serde(default)]
    pub plugin_instances: Vec<PluginInstanceContract>,
    #[serde(default)]
    pub midi_learn_mappings: Vec<MidiLearnMappingContract>,
    #[serde(default)]
    pub midi_notes: Vec<MidiNoteContract>,
    /// Musical chord-track events used by the composition view and generators.
    #[serde(default)]
    pub chord_track: Vec<ChordEvent>,
    /// Sample-accurate control-plane MIDI, including bounded SysEx and MIDI
    /// 2.0 events.  Scheduled notes remain a separate musical model, while
    /// this stream preserves automation/controller messages across save,
    /// history checkout, and application restart.
    #[serde(default)]
    pub midi_events: Vec<MIDIEvent>,
    #[serde(default)]
    pub tempo_events: Vec<TempoEventContract>,
    #[serde(default)]
    pub time_signature_events: Vec<TimeSignatureEventContract>,
    #[serde(default)]
    pub macro_mappings: Vec<MacroMappingContract>,
    #[serde(default)]
    pub warp_markers: Vec<WarpMarkerContract>,
    #[serde(default)]
    pub render_targets: Vec<RenderTargetContract>,
    #[serde(default)]
    pub freeze_artifacts: Vec<FreezeArtifactContract>,
    #[serde(default)]
    pub sidechain_routes: Vec<SidechainRouteContract>,
    #[serde(default)]
    pub feedback_routes: Vec<FeedbackRouteContract>,
    #[serde(default)]
    pub audio_routes: Vec<AudioRouteContract>,
    #[serde(default)]
    pub openutau_vocals: Vec<OpenUtauVocalContract>,
    #[serde(default)]
    pub comp_takes: Vec<CompTakeContract>,
    #[serde(default)]
    pub comp_segments: Vec<CompSegmentContract>,
    #[serde(default)]
    pub track_stacks: Vec<TrackStackContract>,
    #[serde(default)]
    pub markers: Vec<MarkerContract>,
    #[serde(default)]
    pub vca_groups: Vec<VcaGroupContract>,
    /// External hardware inserts are project-scoped routing contracts. The
    /// audio device layer may be unavailable during offline editing, so the
    /// routing and PDC intent must survive save/reload independently.
    #[serde(default)]
    pub hardware_inserts: Vec<HardwareInsert>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ProjectTrack {
    pub id: u32,
    pub name: String,
    pub track_type: String,
    pub volume: f32,
    pub pan: f32,
    pub muted: bool,
    pub solo: bool,
    #[serde(default)]
    pub record_armed: bool,
    #[serde(default)]
    pub phase_invert: bool,
    #[serde(default)]
    pub track_delay_samples: u32,
    #[serde(default)]
    pub volume_automation: Vec<AutomationPointContract>,
    #[serde(default)]
    pub pan_automation: Vec<AutomationPointContract>,
    #[serde(default)]
    pub track_delay_automation: Vec<AutomationPointContract>,
    #[serde(default)]
    pub plugin_types: Vec<u32>,
    /// Bypass is part of the insert-slot state, not a property of the
    /// serialized plugin blob. Keep one entry per plugin slot so a reload
    /// cannot silently enable a plugin that was intentionally bypassed.
    #[serde(default)]
    pub plugin_bypasses: Vec<bool>,
    #[serde(default)]
    pub plugin_parameter_values: Vec<Vec<f32>>,
    #[serde(default)]
    pub plugin_states: Vec<Vec<u8>>,
    #[serde(default)]
    pub plugin_gui_states: Vec<Vec<u8>>,
    #[serde(default)]
    pub plugin_state_versions: Vec<u32>,
    #[serde(default)]
    pub sandbox_plugin_paths: Vec<String>,
    #[serde(default)]
    pub sandbox_plugin_states: Vec<Vec<u8>>,
    #[serde(default)]
    pub sandbox_plugin_state_versions: Vec<u32>,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct ProjectRegion {
    pub id: u32,
    pub track_id: u32,
    pub name: String,
    pub path: String,
    pub start: u64,
    pub length: u64,
    #[serde(default)]
    pub source_offset: u64,
    #[serde(default)]
    pub base_source_offset: u64,
    #[serde(default)]
    pub base_length: u64,
    pub muted: bool,
    pub clip_gain: f32,
    pub fade_in_samples: u64,
    pub fade_out_samples: u64,
    #[serde(default = "default_warp_ratio")]
    pub warp_ratio: f64,
    #[serde(default)]
    pub pitch_semitones: f32,
    #[serde(default)]
    pub reverse: bool,
    #[serde(default = "default_loop_count")]
    pub loop_count: u32,
}

impl ProjectDocument {
    /// Extracts a portable track bundle for transfer between projects.
    pub fn export_track_bundle(&self, track_id: u32) -> Result<Vec<u8>> {
        if track_id == 0 { bail!("track id is invalid"); }
        let track = self.tracks.iter().find(|track| track.id == track_id).cloned().ok_or_else(|| anyhow::anyhow!("track not found"))?;
        let regions = self.regions.iter().filter(|region| region.track_id == track_id).cloned().collect::<Vec<_>>();
        let notes = self.midi_notes.iter().filter(|note| note.track_id == track_id).cloned().collect::<Vec<_>>();
        let payload = serde_json::json!({"format":"aura-track-bundle-v1","track":track,"regions":regions,"midi_notes":notes});
        let payload_bytes = serde_json::to_vec(&payload)?;
        let checksum = format!("{:x}", Sha256::digest(&payload_bytes));
        let bundle = serde_json::json!({"format":"aura-track-bundle-v1","track":payload["track"],"regions":payload["regions"],"midi_notes":payload["midi_notes"],"sha256":checksum});
        Ok(serde_json::to_vec(&bundle)?)
    }

    pub fn import_track_bundle(&mut self, bundle_data: &[u8], new_track_id: u32) -> Result<()> {
        if new_track_id == 0 { bail!("track id is invalid"); }
        let value: serde_json::Value = serde_json::from_slice(bundle_data).context("track bundle JSON is invalid")?;
        if value.get("format").and_then(|v| v.as_str()) != Some("aura-track-bundle-v1") { bail!("unsupported track bundle format"); }
        if let Some(expected) = value.get("sha256").and_then(|v| v.as_str()) {
            let payload = serde_json::json!({"format":"aura-track-bundle-v1","track":value.get("track"),"regions":value.get("regions").cloned().unwrap_or_else(|| serde_json::json!([])),"midi_notes":value.get("midi_notes").cloned().unwrap_or_else(|| serde_json::json!([]))});
            let actual = format!("{:x}", Sha256::digest(serde_json::to_vec(&payload)?));
            if expected != actual { bail!("track bundle checksum mismatch"); }
        }
        let mut track: ProjectTrack = serde_json::from_value(value.get("track").cloned().ok_or_else(|| anyhow::anyhow!("track bundle has no track"))?)?;
        if self.tracks.iter().any(|candidate| candidate.id == new_track_id) { bail!("destination track id already exists"); }
        track.id = new_track_id;
        let mut regions: Vec<ProjectRegion> = serde_json::from_value(value.get("regions").cloned().unwrap_or_else(|| serde_json::json!([])))?;
        let mut notes: Vec<MidiNoteContract> = serde_json::from_value(value.get("midi_notes").cloned().unwrap_or_else(|| serde_json::json!([])))?;
        for region in &mut regions { region.track_id = new_track_id; }
        for note in &mut notes { note.track_id = new_track_id; }
        let mut candidate = self.clone();
        candidate.tracks.push(track); candidate.regions.extend(regions); candidate.midi_notes.extend(notes);
        candidate.validate()?;
        *self = candidate;
        Ok(())
    }
    /// Returns a portable, deterministic identity for the project and its
    /// external audio references.  Paths are retained as authored so the
    /// manifest describes the project contract without leaking host paths.
    pub fn reproducibility_manifest(&self) -> Result<serde_json::Value> {
        let snapshot = serde_json::to_vec(self)?;
        let snapshot_hash = format!("{:x}", Sha256::digest(&snapshot));
        let assets = self.regions.iter().map(|region| {
            serde_json::json!({
                "region_id": region.id,
                "track_id": region.track_id,
                "path": region.path,
                "source_offset": region.source_offset,
                "length": region.length,
            })
        }).collect::<Vec<_>>();
        let asset_bytes = serde_json::to_vec(&assets)?;
        Ok(serde_json::json!({
            "manifest_version": 1,
            "project_id": self.project_id,
            "schema_version": self.schema_version,
            "contract_version": self.contract_version,
            "sample_rate": self.sample_rate,
            "snapshot_sha256": snapshot_hash,
            "asset_reference_sha256": format!("{:x}", Sha256::digest(&asset_bytes)),
            "asset_references": assets,
            "track_count": self.tracks.len(),
            "region_count": self.regions.len(),
            "plugin_count": self.plugin_instances.len(),
        }))
    }
    pub fn from_layout_json(
        name: impl Into<String>,
        bpm: f32,
        sample_rate: f64,
        layout_json: &str,
    ) -> Result<Self> {
        let layout_tracks: Vec<LayoutTrack> =
            serde_json::from_str(layout_json).context("project layout JSON is malformed")?;
        let mut regions = Vec::new();
        let mut tracks = Vec::with_capacity(layout_tracks.len());
        let mut freeze_artifacts = Vec::new();
        let mut sidechain_routes = Vec::new();
        let mut feedback_routes = Vec::new();
        let mut track_ids = HashSet::with_capacity(layout_tracks.len());

        for track in layout_tracks {
            if !track_ids.insert(track.id) {
                bail!("duplicate track id {}", track.id);
            }
            let track_id = track.id;
            for route in &track.sidechain_routes {
                if route.source_id == route.destination_id
                    || route.destination_id != track_id
                    || route.tap_point > 2
                {
                    bail!("invalid sidechain route for track {track_id}");
                }
                sidechain_routes.push(route.clone());
            }
            for route in &track.feedback_routes {
                if route.source_id == route.destination_id
                    || route.source_id != track_id
                    || !route.gain.is_finite()
                    || !(0.0..=2.0).contains(&route.gain)
                {
                    bail!("invalid feedback route for track {track_id}");
                }
                feedback_routes.push(route.clone());
            }
            if track.frozen && !track.frozen_path.trim().is_empty() {
                let bytes = std::fs::read(&track.frozen_path)
                    .with_context(|| format!("frozen audio cache is unreadable: {}", track.frozen_path))?;
                let total_samples = track.frozen_total_samples;
                let sample_rate = track.frozen_sample_rate;
                if total_samples == 0 || sample_rate == 0 {
                    bail!("frozen audio metadata is invalid for track {track_id}");
                }
                freeze_artifacts.push(FreezeArtifactContract {
                    track_id,
                    project_generation: 1,
                    audio_generation: 1,
                    total_samples,
                    sample_rate,
                    path: track.frozen_path.clone(),
                    content_checksum: crate::persistence::PersistenceOrchestrator::calculate_checksum(&bytes),
                });
            }
            tracks.push(ProjectTrack {
                id: track.id,
                name: track.name,
                track_type: track.track_type,
                volume: track.volume,
                pan: track.pan,
                muted: track.muted,
                solo: track.solo,
                record_armed: track.record_armed,
                phase_invert: track.phase_invert,
                track_delay_samples: track.track_delay_samples,
                volume_automation: track.volume_automation.clone(),
                pan_automation: track.pan_automation.clone(),
                track_delay_automation: track.track_delay_automation.clone(),
                plugin_types: track.plugin_types.clone(),
                plugin_bypasses: plugin_bypasses(&track.plugin_bypasses, track.plugin_types.len())?,
                plugin_parameter_values: plugin_parameter_values(
                    &track.plugin_parameter_values,
                    track.plugin_types.len(),
                )?,
                plugin_states: track
                    .plugin_state_hex
                    .iter()
                    .map(|encoded| decode_hex(encoded))
                    .collect::<Result<Vec<_>>>()?,
                plugin_gui_states: {
                    let decoded = track.plugin_gui_state_hex
                        .iter()
                        .map(|encoded| decode_hex(encoded))
                        .collect::<Result<Vec<_>>>()?;
                    if decoded.is_empty() { vec![Vec::new(); track.plugin_types.len()] } else { decoded }
                },
                plugin_state_versions: state_versions(
                    &track.plugin_state_versions,
                    track.plugin_state_hex.len(),
                )?,
                sandbox_plugin_paths: track.sandbox_plugin_paths.clone(),
                sandbox_plugin_states: track
                    .sandbox_plugin_state_hex
                    .iter()
                    .map(|encoded| decode_hex(encoded))
                    .collect::<Result<Vec<_>>>()?,
                sandbox_plugin_state_versions: state_versions(
                    &track.sandbox_plugin_state_versions,
                    track.sandbox_plugin_state_hex.len(),
                )?,
            });
            for region in track.regions {
                regions.push(ProjectRegion {
                    id: region.id,
                    track_id,
                    name: region.name,
                    path: region.path,
                    start: region.start,
                    length: region.len,
                    source_offset: region.source_offset,
                    base_source_offset: region.base_source_offset,
                    base_length: region.base_length,
                    muted: region.muted,
                    clip_gain: region.clip_gain,
                    fade_in_samples: region.fade_in_samples,
                    fade_out_samples: region.fade_out_samples,
                    warp_ratio: region.warp_ratio,
                    pitch_semitones: region.pitch_semitones,
                    reverse: region.reverse,
                    loop_count: region.loop_count,
                });
            }
        }

        let mut plugin_instances = Vec::new();
        for track in &tracks {
            let mut sandbox_index = 0usize;
            for (slot_index, plugin_type) in track.plugin_types.iter().copied().enumerate() {
                let (format, bundle_path, plugin_id, state_blob, capability, binary_hash) =
                    if plugin_type == u32::MAX {
                        let path = track
                            .sandbox_plugin_paths
                            .get(sandbox_index)
                            .cloned()
                            .unwrap_or_default();
                        let id = std::path::Path::new(&path)
                            .file_stem()
                            .and_then(|value| value.to_str())
                            .filter(|value| !value.is_empty())
                            .unwrap_or("external-plugin")
                            .to_owned();
                        let state = track
                            .sandbox_plugin_states
                            .get(sandbox_index)
                            .cloned()
                            .unwrap_or_default();
                        let binary_hash = crate::plugin_catalog::binary_hash_for_path(&path)
                            .unwrap_or_default();
                        sandbox_index += 1;
                        (
                            plugin_format_for_path(&path),
                            path,
                            id,
                            state,
                            "sandbox".to_owned(),
                            binary_hash,
                        )
                    } else {
                        (
                            PluginFormat::BuiltIn,
                            format!("builtin://{plugin_type}"),
                            format!("builtin:{plugin_type}"),
                            track
                                .plugin_states
                                .get(slot_index)
                                .cloned()
                                .unwrap_or_default(),
                            "builtin".to_owned(),
                            String::new(),
                        )
                    };
                plugin_instances.push(PluginInstanceContract {
                    instance_id: format!("track:{}:slot:{}", track.id, slot_index),
                    track_id: track.id,
                    slot_index: slot_index as u32,
                    format,
                    bundle_path,
                    plugin_id,
                    bus_layout: Vec::new(),
                    component_ids: Vec::new(),
                    input_channels: 0,
                    output_channels: 2,
                    sidechain_channels: 0,
                    parameter_ids: Vec::new(),
                    parameter_values: track
                        .plugin_parameter_values
                        .get(slot_index)
                        .cloned()
                        .unwrap_or_default(),
                    latency_samples: 0,
                    state_blob,
                    gui_state: track.plugin_gui_states.get(slot_index).cloned().unwrap_or_default(),
                    bypassed: track.plugin_bypasses.get(slot_index).copied().unwrap_or(false),
                    offline: false,
                    quarantined: false,
                    binary_hash,
                    capability,
                    plugin_version: String::new(),
                    architecture: std::env::consts::ARCH.to_owned(),
                    state_schema_version: PLUGIN_STATE_SCHEMA_VERSION,
                    state_generation: 0,
                });
            }
        }

        let document = Self {
            schema_version: PROJECT_SCHEMA_VERSION,
            contract_version: PROJECT_CONTRACT_VERSION,
            project_id: new_project_id(),
            metadata: ProjectMetadata {
                name: name.into(),
                version: PROJECT_SCHEMA_VERSION,
                bpm,
                tracks_count: track_ids.len() as u32,
                key_root: 0,
                scale_type: 0,
            },
            sample_rate,
            master_gain: 1.0,
            cycle_start_sample: 0,
            cycle_end_sample: 0,
            cycle_enabled: false,
            metronome_enabled: false,
            aux_track_ids: Vec::new(),
            tracks,
            regions,
            plugin_instances,
            midi_learn_mappings: Vec::new(),
            midi_notes: Vec::new(),
            chord_track: Vec::new(),
            midi_events: Vec::new(),
            tempo_events: Vec::new(),
            time_signature_events: Vec::new(),
            macro_mappings: Vec::new(),
            warp_markers: Vec::new(),
            render_targets: Vec::new(),
            freeze_artifacts,
            sidechain_routes,
            feedback_routes,
            audio_routes: Vec::new(),
            openutau_vocals: Vec::new(),
            comp_takes: Vec::new(),
            comp_segments: Vec::new(),
            track_stacks: Vec::new(),
            markers: Vec::new(),
            vca_groups: Vec::new(),
            hardware_inserts: Vec::new(),
        };
        document.validate()?;
        Ok(document)
    }

    pub fn validate(&self) -> Result<()> {
        if self.schema_version != PROJECT_SCHEMA_VERSION {
            bail!("unsupported project schema version {}", self.schema_version);
        }
        if self.metadata.version != PROJECT_SCHEMA_VERSION {
            bail!("metadata version does not match project schema");
        }
        if self.contract_version != PROJECT_CONTRACT_VERSION {
            bail!(
                "unsupported project contract version {}",
                self.contract_version
            );
        }
        if Uuid::parse_str(&self.project_id).is_err() {
            bail!("project id must be a UUID");
        }
        if self.metadata.name.trim().is_empty() {
            bail!("project name must not be empty");
        }
        if !self.metadata.bpm.is_finite() || !(20.0..=300.0).contains(&self.metadata.bpm) {
            bail!("project BPM is outside the supported range");
        }
        if !self.sample_rate.is_finite()
            || self.sample_rate.fract() != 0.0
            || !(1.0..=384_000.0).contains(&self.sample_rate)
        {
            bail!("project sample rate is invalid");
        }
        if !self.master_gain.is_finite() || !(0.0..=2.0).contains(&self.master_gain) {
            bail!("project master gain is invalid");
        }
        if self.cycle_enabled && self.cycle_start_sample >= self.cycle_end_sample {
            bail!("project cycle range is invalid");
        }
        for vocal in &self.openutau_vocals {
            crate::openutau::validate_source(&vocal.source_path)
                .map_err(|error| anyhow::anyhow!(error))?;
            crate::openutau::validate_render(&vocal.rendered_audio_path)
                .map_err(|error| anyhow::anyhow!(error))?;
            let tuning = [
                vocal.tuning.scoop,
                vocal.tuning.vibrato,
                vocal.tuning.dynamics,
                vocal.tuning.consonants,
            ];
            if tuning.iter().any(|value| !value.is_finite() || !(0.0..=1.0).contains(value)) {
                bail!("OpenUtau vocal tuning values are invalid");
            }
        }
        if self.metadata.tracks_count as usize != self.tracks.len() {
            bail!("metadata track count does not match track data");
        }
        if self.tracks.len() > MAX_PROJECT_TRACKS {
            bail!("project contains too many tracks");
        }
        if self.regions.len() > MAX_PROJECT_REGIONS {
            bail!("project contains too many regions");
        }
        if self.plugin_instances.len() > MAX_PROJECT_PLUGIN_INSTANCES {
            bail!("project contains too many plugin instances");
        }
        if self.comp_takes.len() > MAX_PROJECT_COMP_TAKES {
            bail!("project contains too many comp takes");
        }
        if self.comp_segments.len() > MAX_PROJECT_COMP_SEGMENTS {
            bail!("project contains too many comp segments");
        }
        if self.freeze_artifacts.len() > MAX_PROJECT_TRACKS {
            bail!("project contains too many freeze artifacts");
        }
        if self.sidechain_routes.len() > MAX_PROJECT_PLUGIN_INSTANCES {
            bail!("project contains too many sidechain routes");
        }
        if self.feedback_routes.len() > 16 {
            bail!("project contains too many feedback routes");
        }
        let track_ids: HashSet<u32> = self.tracks.iter().map(|track| track.id).collect();
        if self.audio_routes.len() > MAX_PROJECT_TRACKS {
            bail!("project contains too many audio routes");
        }
        let mut audio_route_keys = HashSet::with_capacity(self.audio_routes.len());
        for route in &self.audio_routes {
            if route.source_id == route.destination_id
                || !track_ids.contains(&route.source_id)
                || !track_ids.contains(&route.destination_id)
                || !route.gain.is_finite()
                || !(0.0..=2.0).contains(&route.gain)
                || !audio_route_keys.insert((route.source_id, route.destination_id))
            {
                bail!("invalid or duplicate audio route");
            }
        }
        let mut sidechain_keys = HashSet::with_capacity(self.sidechain_routes.len());
        for route in &self.sidechain_routes {
            if route.source_id == 0
                || route.destination_id == 0
                || route.source_id == route.destination_id
                || !track_ids.contains(&route.source_id)
                || !track_ids.contains(&route.destination_id)
                || route.plugin_index > 65_535
                || route.tap_point > 2
                || !sidechain_keys.insert((route.destination_id, route.plugin_index))
            {
                bail!("invalid or duplicate sidechain route");
            }
        }
        let mut feedback_keys = HashSet::with_capacity(self.feedback_routes.len());
        for route in &self.feedback_routes {
            if route.source_id == 0
                || route.destination_id == 0
                || route.source_id == route.destination_id
                || !track_ids.contains(&route.source_id)
                || !track_ids.contains(&route.destination_id)
                || !route.gain.is_finite()
                || !(0.0..=2.0).contains(&route.gain)
                || !feedback_keys.insert((route.source_id, route.destination_id))
            {
                bail!("invalid or duplicate feedback route");
            }
        }
        let mut freeze_track_ids = HashSet::with_capacity(self.freeze_artifacts.len());
        let mut total_freeze_bytes = 0u64;
        for artifact in &self.freeze_artifacts {
            if artifact.track_id == 0
                || !track_ids.contains(&artifact.track_id)
                || !freeze_track_ids.insert(artifact.track_id)
                || artifact.project_generation == 0
                || artifact.audio_generation == 0
                || artifact.total_samples == 0
                || artifact.total_samples > MAX_FREEZE_SAMPLES
                || artifact.sample_rate == 0
                || artifact.path.trim().is_empty()
                || artifact.path.len() > 4096
                || artifact.path.contains('\0')
                || artifact.content_checksum == 0
            {
                bail!("invalid freeze artifact metadata for track {}", artifact.track_id);
            }
            total_freeze_bytes = total_freeze_bytes
                .checked_add(artifact.total_samples.saturating_mul(8))
                .ok_or_else(|| anyhow::anyhow!("freeze artifact size overflows"))?;
            if total_freeze_bytes > MAX_FREEZE_CACHE_BYTES {
                bail!("freeze cache budget exceeded");
            }
        }
        let mut comp_take_ids = HashSet::with_capacity(self.comp_takes.len());
        for take in &self.comp_takes {
            take.validate()?;
            if !comp_take_ids.insert(take.id) {
                bail!("duplicate comp take id {}", take.id);
            }
        }
        let mut sorted_segments = self.comp_segments.clone();
        sorted_segments.sort_by_key(|segment| segment.start_sample);
        for segment in &sorted_segments {
            segment.validate()?;
            if !comp_take_ids.contains(&segment.take_id) {
                bail!("comp segment references unknown take {}", segment.take_id);
            }
        }
        if sorted_segments.windows(2).any(|pair| {
            pair[0].start_sample.checked_add(pair[0].length_samples)
                .is_none_or(|end| end > pair[1].start_sample)
        }) {
            bail!("comp segments overlap");
        }

        let mut total_state_bytes = 0usize;
        let mut total_string_bytes = self.metadata.name.len() + self.project_id.len();
        for track in &self.tracks {
            total_string_bytes = total_string_bytes
                .checked_add(track.name.len())
                .and_then(|value| {
                    track
                        .sandbox_plugin_paths
                        .iter()
                        .try_fold(value, |sum, path| sum.checked_add(path.len()))
                })
                .ok_or_else(|| anyhow::anyhow!("project string data size overflow"))?;
            for state in track.plugin_states.iter().chain(track.sandbox_plugin_states.iter()) {
                total_state_bytes = total_state_bytes
                    .checked_add(state.len())
                    .ok_or_else(|| anyhow::anyhow!("project plugin state size overflow"))?;
            }
        }
        for region in &self.regions {
            total_string_bytes = total_string_bytes
                .checked_add(region.name.len())
                .and_then(|value| value.checked_add(region.path.len()))
                .ok_or_else(|| anyhow::anyhow!("project string data size overflow"))?;
        }
        for plugin in &self.plugin_instances {
            total_string_bytes = total_string_bytes
                .checked_add(plugin.instance_id.len())
                .and_then(|value| value.checked_add(plugin.bundle_path.len()))
                .and_then(|value| value.checked_add(plugin.plugin_id.len()))
                .and_then(|value| value.checked_add(plugin.binary_hash.len()))
                .and_then(|value| value.checked_add(plugin.capability.len()))
                .and_then(|value| value.checked_add(plugin.plugin_version.len()))
                .and_then(|value| value.checked_add(plugin.architecture.len()))
                .and_then(|value| {
                    plugin.component_ids.iter().try_fold(value, |total, component| {
                        total.checked_add(component.len())
                    })
                })
                .ok_or_else(|| anyhow::anyhow!("project string data size overflow"))?;
            total_state_bytes = total_state_bytes
                .checked_add(plugin.state_blob.len())
                .and_then(|value| value.checked_add(plugin.gui_state.len()))
                .ok_or_else(|| anyhow::anyhow!("project plugin state size overflow"))?;
            if plugin.parameter_ids.len() > MAX_PLUGIN_PARAMETERS {
                bail!("plugin parameter metadata is too large");
            }
            if plugin.parameter_values.len() > MAX_PLUGIN_PARAMETERS
                || plugin
                    .parameter_values
                    .iter()
                    .any(|value| !value.is_finite() || !(0.0..=1.0).contains(value))
            {
                bail!("plugin parameter values are invalid");
            }
        }
        if total_state_bytes > MAX_PROJECT_STATE_BYTES {
            bail!("project plugin state data exceeds aggregate limit");
        }
        if total_string_bytes > MAX_PROJECT_STRING_BYTES {
            bail!("project string data exceeds aggregate limit");
        }
        if self.midi_events.len() > MAX_PROJECT_REGIONS {
            bail!("project MIDI event count exceeds aggregate limit");
        }
        if self.midi_events.iter().any(|event| !event.validate()) {
            bail!("project contains invalid MIDI, SysEx, or MIDI 2.0 event");
        }
        let midi_payload_bytes = self
            .midi_events
            .iter()
            .try_fold(0usize, |total, event| {
                let payload = match &event.kind {
                    MIDIEventKind::SysEx { data } => data.len(),
                    _ => 0,
                };
                total.checked_add(payload)
            })
            .ok_or_else(|| anyhow::anyhow!("project MIDI payload size overflow"))?;
        if midi_payload_bytes > MAX_PROJECT_MIDI_BYTES {
            bail!("project MIDI payload exceeds aggregate limit");
        }

        let mut track_ids = HashSet::with_capacity(self.tracks.len());
        for track in &self.tracks {
            if track.id == 0 {
                bail!("track id must be non-zero");
            }
            if !track_ids.insert(track.id) {
                bail!("duplicate track id {}", track.id);
            }
            if track.plugin_types.len() != track.plugin_states.len() {
                bail!("track {} plugin type/state counts differ", track.id);
            }
            if track.plugin_bypasses.len() != track.plugin_types.len() {
                bail!("track {} plugin bypass/type counts differ", track.id);
            }
            if track.plugin_parameter_values.len() != track.plugin_types.len() {
                bail!("track {} plugin parameter/type counts differ", track.id);
            }
            if track.plugin_gui_states.len() != track.plugin_types.len() {
                bail!("track {} plugin GUI state/type counts differ", track.id);
            }
            if track.plugin_gui_states.iter().any(|state| state.len() > 1024 * 1024) {
                bail!("track {} contains oversized plugin GUI state", track.id);
            }
            if track.plugin_parameter_values.iter().flatten().any(|value| {
                !value.is_finite() || !(0.0..=1.0).contains(value)
            }) {
                bail!("track {} contains invalid plugin parameter values", track.id);
            }
            if track.plugin_state_versions.len() != track.plugin_states.len() {
                bail!("track {} plugin state version counts differ", track.id);
            }
            if track
                .plugin_state_versions
                .iter()
                .any(|version| *version != PLUGIN_STATE_SCHEMA_VERSION)
            {
                bail!(
                    "track {} contains an unsupported plugin state version",
                    track.id
                );
            }
            if track
                .plugin_states
                .iter()
                .any(|state| state.len() > 4 * 1024 * 1024)
            {
                bail!("track {} contains oversized plugin state", track.id);
            }
            if track.sandbox_plugin_paths.len() != track.sandbox_plugin_states.len() {
                bail!("track {} sandbox path/state counts differ", track.id);
            }
            if track.sandbox_plugin_state_versions.len() != track.sandbox_plugin_states.len() {
                bail!(
                    "track {} sandbox plugin state version counts differ",
                    track.id
                );
            }
            if track
                .sandbox_plugin_state_versions
                .iter()
                .any(|version| *version != PLUGIN_STATE_SCHEMA_VERSION)
            {
                bail!(
                    "track {} contains an unsupported sandbox plugin state version",
                    track.id
                );
            }
            let sandbox_slots = track
                .plugin_types
                .iter()
                .filter(|plugin_type| **plugin_type == u32::MAX)
                .count();
            if sandbox_slots != track.sandbox_plugin_paths.len() {
                bail!(
                    "track {} sandbox plugin metadata does not match plugin types",
                    track.id
                );
            }
            if track
                .sandbox_plugin_states
                .iter()
                .any(|state| state.len() > 4 * 1024 * 1024)
            {
                bail!("track {} contains oversized sandbox plugin state", track.id);
            }
            if track
                .sandbox_plugin_paths
                .iter()
                .any(|path| path.trim().is_empty() || path.contains('\0'))
            {
                bail!("track {} contains an invalid sandbox plugin path", track.id);
            }
            if track.name.trim().is_empty()
                || track.name.contains('\0')
                || !track.volume.is_finite()
                || !(0.0..=2.0).contains(&track.volume)
                || !track.pan.is_finite()
                || !(-1.0..=1.0).contains(&track.pan)
                || track.track_delay_samples > 8192
            {
                bail!("track {} contains invalid data", track.id);
            }
            Self::native_track_type_code(&track.track_type)?;
        }

        let mut stack_ids = HashSet::with_capacity(self.track_stacks.len());
        for stack in &self.track_stacks {
            if stack.id == 0 || !stack_ids.insert(stack.id) || stack.name.trim().is_empty()
                || stack.name.len() > 256 || !stack.master_gain.is_finite()
                || !(0.0..=2.0).contains(&stack.master_gain)
            {
                bail!("track stack metadata is invalid");
            }
            let mut members = HashSet::with_capacity(stack.member_track_ids.len());
            if stack.member_track_ids.iter().any(|track_id| {
                *track_id == 0 || !track_ids.contains(track_id) || !members.insert(*track_id)
            }) {
                bail!("track stack {} contains invalid or duplicate members", stack.id);
            }
        }
        if self.markers.len() > 65_536 {
            bail!("too many arrangement markers");
        }
        let mut marker_ids = HashSet::with_capacity(self.markers.len());
        for marker in &self.markers {
            marker.validate()?;
            if !marker_ids.insert(marker.id) {
                bail!("duplicate arrangement marker id {}", marker.id);
            }
        }
        if self.vca_groups.len() > 2048 {
            bail!("too many VCA groups");
        }
        let mut aux_ids = HashSet::with_capacity(self.aux_track_ids.len());
        for aux_id in &self.aux_track_ids {
            if *aux_id == 0 || !aux_ids.insert(*aux_id) || !track_ids.contains(aux_id) {
                bail!("Aux track metadata references an invalid or duplicate track");
            }
            let track = self.tracks.iter().find(|track| track.id == *aux_id).expect("track id checked");
            if !matches!(track.track_type.as_str(), "Aux" | "Bus") {
                bail!("Aux metadata references a non-bus track {}", aux_id);
            }
        }
        let mut vca_ids = HashSet::with_capacity(self.vca_groups.len());
        for group in &self.vca_groups {
            group.validate()?;
            if !vca_ids.insert(group.id) || group.track_ids.iter().any(|id| !track_ids.contains(id)) {
                bail!("VCA group {} contains an invalid or duplicate track", group.id);
            }
        }

        // Region IDs cross the FFI boundary without a track namespace. Keep
        // them globally unique so a reload cannot bind a command to the
        // wrong region when tracks are reordered or hydrated incrementally.
        let mut region_ids = HashSet::with_capacity(self.regions.len());
        for region in &self.regions {
            if region.id == 0 {
                bail!("region id must be non-zero");
            }
            if !track_ids.contains(&region.track_id) {
                bail!("region {} references an unknown track", region.id);
            }
            if region.path.trim().is_empty() || region.path.contains('\0') || region.length == 0 {
                bail!("region {} has invalid media data", region.id);
            }
            if region.start > u64::MAX - region.length {
                bail!("region {} exceeds the project timeline", region.id);
            }
            if region.base_length == 0 {
                if region.source_offset != 0 || region.base_source_offset != 0 {
                    bail!("region {} has source trim without a base length", region.id);
                }
            } else if region.source_offset < region.base_source_offset
                || region
                    .source_offset
                    .checked_sub(region.base_source_offset)
                    .and_then(|offset| offset.checked_add(region.length))
                    .is_none_or(|end| end > region.base_length)
            {
                bail!("region {} source trim exceeds its source bounds", region.id);
            }
            if region.fade_in_samples > region.length
                || region.fade_out_samples > region.length
                || !region.clip_gain.is_finite()
                || !(0.0..=2.0).contains(&region.clip_gain)
                || !region.warp_ratio.is_finite()
                || !(0.5..=2.0).contains(&region.warp_ratio)
                || !region.pitch_semitones.is_finite()
                || !(-24.0..=24.0).contains(&region.pitch_semitones)
                || !(1..=1024).contains(&region.loop_count)
            {
                bail!("region {} contains invalid envelope data", region.id);
            }
            if !region_ids.insert(region.id) {
                bail!("duplicate region id {}", region.id,);
            }
        }
        let mut plugin_slots = HashSet::with_capacity(self.plugin_instances.len());
        for plugin in &self.plugin_instances {
            if plugin.track_id != 0 {
                let Some(track) = self.tracks.iter().find(|track| track.id == plugin.track_id) else {
                    bail!(
                        "plugin instance {} references unknown track {}",
                        plugin.instance_id,
                        plugin.track_id
                    );
                };
                if plugin.slot_index as usize >= track.plugin_types.len() {
                    bail!(
                        "plugin instance {} references missing slot {} on track {}",
                        plugin.instance_id,
                        plugin.slot_index,
                        plugin.track_id
                    );
                }
                if !plugin_slots.insert((plugin.track_id, plugin.slot_index)) {
                    bail!(
                        "duplicate plugin slot {} on track {}",
                        plugin.slot_index,
                        plugin.track_id
                    );
                }
            }
        }
        let automation_points = self.tracks.iter().try_fold(0usize, |total, track| {
            let count = track.volume_automation.len()
                .checked_add(track.pan_automation.len())
                .and_then(|count| count.checked_add(track.track_delay_automation.len()))
                .ok_or_else(|| anyhow::anyhow!("automation point count overflow"))?;
            total.checked_add(count).ok_or_else(|| anyhow::anyhow!("automation point count overflow"))
        })?;
        if automation_points > MAX_PROJECT_REGIONS {
            bail!("project contains too many automation points");
        }
        for track in &self.tracks {
            for points in [&track.volume_automation, &track.pan_automation, &track.track_delay_automation] {
                let mut previous = None;
                for point in points {
                    point.validate()?;
                    if previous.is_some_and(|time| point.time <= time) {
                        bail!("track {} automation points are not strictly ordered", track.id);
                    }
                    previous = Some(point.time);
                }
            }
        }
        if self.midi_notes.len() > MAX_PROJECT_REGIONS {
            bail!("project contains too many MIDI notes");
        }
        for note in &self.midi_notes {
            note.validate()?;
            if note.probability > 100 || note.repeat_count == 0 {
                bail!("MIDI note probability/repeat attributes are invalid");
            }
            if note.lyric.len() > 1_024 || note.lyric.contains('\0') {
                bail!("MIDI note lyric must be at most 1024 bytes and contain no NUL");
            }
            if !track_ids.contains(&note.track_id) {
                bail!("MIDI note references unknown track {}", note.track_id);
            }
        }
        if self.chord_track.len() > MAX_PROJECT_REGIONS {
            bail!("project contains too many chord events");
        }
        let mut previous_chord_tick = None;
        for chord in &self.chord_track {
            if previous_chord_tick.is_some_and(|tick| chord.tick < tick)
                || chord.root > 127
                || chord.intervals.len() > 32
                || chord.intervals.iter().any(|interval| *interval > 127)
                || chord.name.trim().is_empty()
                || chord.name.chars().count() > 128
                || chord.name.contains('\0')
            {
                bail!("invalid chord-track event");
            }
            previous_chord_tick = Some(chord.tick);
        }
        if self.tempo_events.len() > 65_536 {
            bail!("project contains too many tempo events");
        }
        let mut previous_beat = None;
        for event in &self.tempo_events {
            event.validate()?;
            if previous_beat.is_some_and(|beat| event.beat <= beat) {
                bail!("tempo events are not strictly ordered");
            }
            previous_beat = Some(event.beat);
        }
        if self.time_signature_events.len() > 65_536 {
            bail!("project contains too many time signature events");
        }
        let mut previous_signature_beat = None;
        for event in &self.time_signature_events {
            event.validate()?;
            if previous_signature_beat.is_some_and(|beat| event.beat <= beat) {
                bail!("time signature events are not strictly ordered");
            }
            previous_signature_beat = Some(event.beat);
        }
        validate_contracts(
            &self.plugin_instances,
            &self.midi_learn_mappings,
            &self.macro_mappings,
            &self.warp_markers,
            &self.render_targets,
        )?;
        if self.hardware_inserts.len() > 256 {
            bail!("project contains too many hardware inserts");
        }
        for insert in &self.hardware_inserts {
            insert.validate().map_err(|error| anyhow::anyhow!(error))?;
        }
        Ok(())
    }

    /// Converts the persisted track label to the Native Engine enum value.
    /// Keeping this mapping here prevents the FFI hydration path from silently
    /// falling back to Audio for an unknown or misspelled track type.
    pub fn native_track_type_code(track_type: &str) -> Result<u32> {
        match track_type {
            "Audio" => Ok(0),
            "Midi" => Ok(1),
            "Instrument" => Ok(2),
                "Bus" | "Aux" => Ok(3),
            "Vocal" => Ok(4),
            _ => bail!("unsupported track type {track_type:?}"),
        }
    }

    /// Add a clean, empty track to an offline project document. IDs are
    /// monotonic within the document and the type is validated before any
    /// mutation is published.
    pub fn add_track(&mut self, name: impl Into<String>, track_type: &str) -> Result<u32> {
        let name = name.into();
        if name.trim().is_empty() || name.len() > 1024 { bail!("track name is invalid"); }
        Self::native_track_type_code(track_type)?;
        let id = self.tracks.iter().map(|track| track.id).max().unwrap_or(0)
            .checked_add(1).ok_or_else(|| anyhow::anyhow!("track id space exhausted"))?;
        self.tracks.push(ProjectTrack {
            id, name, track_type: track_type.to_owned(), volume: 1.0, pan: 0.0,
            muted: false, solo: false, record_armed: false, phase_invert: false,
            track_delay_samples: 0, volume_automation: Vec::new(), pan_automation: Vec::new(),
            track_delay_automation: Vec::new(), plugin_types: Vec::new(), plugin_bypasses: Vec::new(),
            plugin_parameter_values: Vec::new(), plugin_states: Vec::new(), plugin_gui_states: Vec::new(),
            plugin_state_versions: Vec::new(), sandbox_plugin_paths: Vec::new(),
            sandbox_plugin_states: Vec::new(), sandbox_plugin_state_versions: Vec::new(),
        });
        self.metadata.tracks_count = self.tracks.len() as u32;
        Ok(id)
    }

    /// Rename an existing track while preserving its stable identity.
    pub fn set_track_name(&mut self, track_id: u32, name: impl Into<String>) -> Result<()> {
        let name = name.into();
        if name.trim().is_empty() || name.len() > 1024 {
            bail!("track name is invalid");
        }
        let track = self
            .tracks
            .iter_mut()
            .find(|track| track.id == track_id)
            .ok_or_else(|| anyhow::anyhow!("track was not found"))?;
        track.name = name;
        Ok(())
    }

    /// Remove a track and all project records owned by it.
    pub fn remove_track(&mut self, track_id: u32) -> Result<()> {
        let before = self.tracks.len();
        self.tracks.retain(|track| track.id != track_id);
        if self.tracks.len() == before {
            bail!("track was not found");
        }
        self.aux_track_ids.retain(|id| *id != track_id);
        self.regions.retain(|region| region.track_id != track_id);
        self.midi_notes.retain(|note| note.track_id != track_id);
        self.render_targets.retain(|target| target.source_id != track_id);
        self.audio_routes.retain(|route| {
            route.source_id != track_id && route.destination_id != track_id
        });
        self.sidechain_routes.retain(|route| {
            route.source_id != track_id && route.destination_id != track_id
        });
        self.feedback_routes.retain(|route| {
            route.source_id != track_id && route.destination_id != track_id
        });
        self.metadata.tracks_count = self.tracks.len() as u32;
        Ok(())
    }

    pub fn insert_builtin_plugin(&mut self, track_id: u32, plugin_type: u32) -> Result<u32> {
        if plugin_type == 0 { bail!("plugin type must be non-zero"); }
        let track = self.tracks.iter_mut().find(|track| track.id == track_id)
            .ok_or_else(|| anyhow::anyhow!("track was not found"))?;
        let slot = track.plugin_types.len() as u32;
        if slot >= 1024 { bail!("plugin slot limit exceeded"); }
        track.plugin_types.push(plugin_type);
        track.plugin_bypasses.push(false);
        track.plugin_parameter_values.push(Vec::new());
        track.plugin_states.push(Vec::new());
        track.plugin_gui_states.push(Vec::new());
        track.plugin_state_versions.push(1);
        Ok(slot)
    }

    pub fn save_atomic(&self, path: &str) -> Result<()> {
        self.validate()?;
        SovereignPersistence::save_document(path, self)
    }

    pub fn load(path: &str) -> Result<Self> {
        let document: Self = SovereignPersistence::load_json(path)?;
        document.validate()?;
        Ok(document)
    }
}

fn default_contract_version() -> u32 {
    PROJECT_CONTRACT_VERSION
}

fn new_project_id() -> String {
    Uuid::new_v4().to_string()
}

#[derive(Debug, Deserialize)]
struct LayoutTrack {
    id: u32,
    #[serde(default)]
    name: String,
    #[serde(rename = "type", default = "default_track_type")]
    track_type: String,
    #[serde(default = "default_volume")]
    volume: f32,
    #[serde(default)]
    pan: f32,
    #[serde(default, alias = "mute")]
    muted: bool,
    #[serde(default)]
    solo: bool,
    #[serde(default, alias = "recordArmed")]
    record_armed: bool,
    #[serde(default, alias = "phaseInvert")]
    phase_invert: bool,
    #[serde(default, alias = "trackDelaySamples")]
    track_delay_samples: u32,
    #[serde(default)]
    frozen: bool,
    #[serde(default)]
    frozen_total_samples: u64,
    #[serde(default)]
    frozen_sample_rate: u32,
    #[serde(default)]
    frozen_path: String,
    #[serde(default)]
    volume_automation: Vec<AutomationPointContract>,
    #[serde(default)]
    pan_automation: Vec<AutomationPointContract>,
    #[serde(default, alias = "trackDelayAutomation")]
    track_delay_automation: Vec<AutomationPointContract>,
    #[serde(default)]
    plugin_types: Vec<u32>,
    #[serde(default, rename = "plugin_bypass", alias = "pluginBypass", alias = "plugin_bypasses")]
    plugin_bypasses: Vec<bool>,
    #[serde(default, alias = "pluginParameterValues")]
    plugin_parameter_values: Vec<Vec<f32>>,
    #[serde(default)]
    plugin_state_hex: Vec<String>,
    #[serde(default)]
    plugin_gui_state_hex: Vec<String>,
    #[serde(default)]
    plugin_state_versions: Vec<u32>,
    #[serde(default)]
    sandbox_plugin_paths: Vec<String>,
    #[serde(default)]
    sandbox_plugin_state_hex: Vec<String>,
    #[serde(default)]
    sandbox_plugin_state_versions: Vec<u32>,
    #[serde(default)]
    sidechain_routes: Vec<SidechainRouteContract>,
    #[serde(default)]
    feedback_routes: Vec<FeedbackRouteContract>,
    #[serde(default)]
    regions: Vec<LayoutRegion>,
}

fn decode_hex(value: &str) -> Result<Vec<u8>> {
    if !value.len().is_multiple_of(2) {
        bail!("plugin state hex has odd length");
    }
    let mut bytes = Vec::with_capacity(value.len() / 2);
    for pair in value.as_bytes().as_chunks::<2>().0 {
        let high = (pair[0] as char)
            .to_digit(16)
            .ok_or_else(|| anyhow::anyhow!("plugin state contains invalid hex"))?;
        let low = (pair[1] as char)
            .to_digit(16)
            .ok_or_else(|| anyhow::anyhow!("plugin state contains invalid hex"))?;
        bytes.push(((high << 4) | low) as u8);
    }
    Ok(bytes)
}

fn state_versions(versions: &[u32], state_count: usize) -> Result<Vec<u32>> {
    if versions.is_empty() {
        return Ok(vec![PLUGIN_STATE_SCHEMA_VERSION; state_count]);
    }
    if versions.len() != state_count {
        bail!("plugin state version count does not match state count");
    }
    versions
        .iter()
        .map(|version| match *version {
            // Version zero was the pre-versioned raw plugin blob. Its wire
            // bytes are unchanged, so migration is metadata-only and safe.
            0 | PLUGIN_STATE_SCHEMA_VERSION => Ok(PLUGIN_STATE_SCHEMA_VERSION),
            other => bail!("unsupported plugin state version {other}"),
        })
        .collect()
}

fn plugin_bypasses(values: &[bool], plugin_count: usize) -> Result<Vec<bool>> {
    if values.is_empty() {
        return Ok(vec![false; plugin_count]);
    }
    if values.len() != plugin_count {
        bail!("plugin bypass/type counts differ");
    }
    Ok(values.to_vec())
}

fn plugin_parameter_values(values: &[Vec<f32>], plugin_count: usize) -> Result<Vec<Vec<f32>>> {
    if values.is_empty() {
        return Ok(vec![Vec::new(); plugin_count]);
    }
    if values.len() != plugin_count {
        bail!("plugin parameter/type counts differ");
    }
    if values.iter().any(|parameters| parameters.len() > MAX_PLUGIN_PARAMETERS) {
        bail!("plugin parameter values exceed aggregate limit");
    }
    Ok(values.to_vec())
}

#[derive(Debug, Deserialize)]
struct LayoutRegion {
    id: u32,
    #[serde(default)]
    name: String,
    path: String,
    start: u64,
    len: u64,
    #[serde(default)]
    source_offset: u64,
    #[serde(default)]
    base_source_offset: u64,
    #[serde(default)]
    base_length: u64,
    #[serde(default)]
    muted: bool,
    #[serde(default = "default_volume")]
    clip_gain: f32,
    #[serde(default)]
    fade_in_samples: u64,
    #[serde(default)]
    fade_out_samples: u64,
    #[serde(default = "default_warp_ratio")]
    warp_ratio: f64,
    #[serde(default)]
    pitch_semitones: f32,
    #[serde(default)]
    reverse: bool,
    #[serde(default = "default_loop_count")]
    loop_count: u32,
}

fn default_track_type() -> String {
    "Audio".to_string()
}

fn default_volume() -> f32 {
    1.0
}

fn default_master_gain() -> f32 {
    1.0
}

fn default_warp_ratio() -> f64 {
    1.0
}

fn default_loop_count() -> u32 {
    1
}

fn plugin_format_for_path(path: &str) -> PluginFormat {
    match std::path::Path::new(path)
        .extension()
        .and_then(|value| value.to_str())
        .map(|value| value.to_ascii_lowercase())
        .as_deref()
    {
        Some("clap") => PluginFormat::Clap,
        Some("vst3") => PluginFormat::Vst3,
        Some("component") => PluginFormat::AudioUnit,
        _ => PluginFormat::BuiltIn,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{SystemTime, UNIX_EPOCH};

    fn temp_path(suffix: &str) -> std::path::PathBuf {
        std::env::temp_dir().join(format!(
            "aura-project-test-{}-{}.{}",
            std::process::id(),
            SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_nanos(),
            suffix
        ))
    }

    #[test]
    fn layout_round_trips_all_project_state() {
        let layout = r#"[{"id":7,"name":"Keys","type":"Instrument","volume":0.8,"pan":-0.2,"mute":false,"solo":true,"record_armed":true,"phase_invert":true,"volume_automation":[{"time":0.0,"value":0.5,"curve":0.0},{"time":1.0,"value":0.8,"curve":0.1}],"pan_automation":[{"time":0.0,"value":0.5,"curve":0.0}],"plugin_types":[0],"plugin_bypass":[true],"plugin_parameter_values":[[0.25]],"plugin_state_hex":["00ff10"],"regions":[{"id":3,"name":"Take","start":128,"len":2048,"source_offset":256,"base_source_offset":0,"base_length":4096,"muted":false,"path":"audio/take.wav","clip_gain":0.75,"fade_in_samples":8,"fade_out_samples":16,"reverse":true,"warp_ratio":1.25,"pitch_semitones":-3.0,"loop_count":3}]}]"#;
        let document = ProjectDocument::from_layout_json("Demo", 128.0, 48_000.0, layout).unwrap();
        assert_eq!(document.tracks[0].plugin_types, vec![0]);
        assert_eq!(
            document.tracks[0].plugin_states,
            vec![vec![0x00, 0xff, 0x10]]
        );
        assert_eq!(document.tracks[0].plugin_bypasses, vec![true]);
        assert_eq!(document.tracks[0].plugin_parameter_values, vec![vec![0.25]]);
        assert_eq!(document.plugin_instances.len(), 1);
        assert_eq!(document.plugin_instances[0].track_id, 7);
        assert_eq!(document.plugin_instances[0].slot_index, 0);
        assert_eq!(document.plugin_instances[0].plugin_id, "builtin:0");
        assert_eq!(document.plugin_instances[0].capability, "builtin");
        assert!(document.plugin_instances[0].bypassed);
        assert_eq!(document.plugin_instances[0].parameter_values, vec![0.25]);
        assert!(document.tracks[0].record_armed);
        assert!(document.tracks[0].phase_invert);
        assert_eq!(document.tracks[0].volume_automation.len(), 2);
        assert_eq!(document.tracks[0].pan_automation.len(), 1);
        assert!(document.regions[0].reverse);
        assert_eq!(document.regions[0].source_offset, 256);
        assert_eq!(document.regions[0].base_length, 4096);
        let path = temp_path("aura");
        document.save_atomic(path.to_str().unwrap()).unwrap();
        let loaded = ProjectDocument::load(path.to_str().unwrap()).unwrap();
        assert_eq!(loaded, document);
        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn track_rename_and_remove_preserve_document_invariants() {
        let mut document = ProjectDocument::from_layout_json("Ops", 120.0, 48_000.0, "[]").unwrap();
        let first = document.add_track("Audio", "Audio").unwrap();
        let second = document.add_track("Bus", "Bus").unwrap();
        document.set_track_name(first, "Lead Vocal").unwrap();
        assert_eq!(document.tracks.iter().find(|t| t.id == first).unwrap().name, "Lead Vocal");
        document.remove_track(first).unwrap();
        assert!(document.tracks.iter().all(|t| t.id != first));
        assert_eq!(document.tracks.len(), 1);
        assert!(document.tracks.iter().any(|t| t.id == second));
        assert!(document.validate().is_ok());
    }

    #[test]
    fn routing_edges_round_trip_with_project_layout() {
        let layout = r#"[
            {"id":7,"name":"Source","type":"Audio","regions":[],
             "feedback_routes":[{"source_id":7,"destination_id":8,"gain":0.5}]},
            {"id":8,"name":"Target","type":"Audio","regions":[],
             "sidechain_routes":[{"source_id":7,"destination_id":8,"plugin_index":1,"tap_point":0}]}
        ]"#;
        let document = ProjectDocument::from_layout_json("Routing", 120.0, 48_000.0, layout).unwrap();
        assert_eq!(document.feedback_routes.len(), 1);
        assert_eq!(document.feedback_routes[0].gain, 0.5);
        assert_eq!(document.sidechain_routes.len(), 1);
        let path = temp_path("routing");
        document.save_atomic(path.to_str().unwrap()).unwrap();
        let loaded = ProjectDocument::load(path.to_str().unwrap()).unwrap();
        assert_eq!(loaded.feedback_routes, document.feedback_routes);
        assert_eq!(loaded.sidechain_routes, document.sidechain_routes);
        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn audio_routes_round_trip_with_canonical_project_document() {
        let layout = r#"[
            {"id":21,"name":"Source","type":"Audio","regions":[]},
            {"id":22,"name":"Bus","type":"Bus","regions":[]}
        ]"#;
        let mut document =
            ProjectDocument::from_layout_json("Audio Routes", 120.0, 48_000.0, layout).unwrap();
        document.audio_routes.push(AudioRouteContract {
            source_id: 21,
            destination_id: 22,
            gain: 0.75,
        });
        document.validate().unwrap();
        let path = temp_path("audio-routes");
        document.save_atomic(path.to_str().unwrap()).unwrap();
        let loaded = ProjectDocument::load(path.to_str().unwrap()).unwrap();
        assert_eq!(loaded.audio_routes, document.audio_routes);
        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn comping_contract_rejects_unknown_take_and_overlapping_segments() {
        let mut document = ProjectDocument::from_layout_json("Comp", 120.0, 48_000.0, "[]")
            .expect("empty project should be valid");
        document.comp_takes.push(CompTakeContract {
            id: 7,
            name: "Vocal take".into(),
            start_sample: 0,
            end_sample: 2_000,
        });
        document.comp_segments = vec![
            CompSegmentContract {
                take_id: 7,
                start_sample: 0,
                length_samples: 1_000,
                crossfade_samples: 32,
            },
            CompSegmentContract {
                take_id: 8,
                start_sample: 1_000,
                length_samples: 1_000,
                crossfade_samples: 32,
            },
        ];
        assert!(document.validate().is_err());

        document.comp_segments[1].take_id = 7;
        document.comp_segments[1].start_sample = 900;
        assert!(document.validate().is_err());
    }

    #[test]
    fn region_pitch_and_warp_are_bounded() {
        let base = r#"[{"id":1,"name":"Audio","regions":[{"id":1,"path":"take.wav","start":0,"len":64,"warp_ratio":2.5,"pitch_semitones":0.0}]}]"#;
        assert!(ProjectDocument::from_layout_json("Demo", 120.0, 48_000.0, base).is_err());

        let valid = r#"[{"id":1,"name":"Audio","regions":[{"id":1,"path":"take.wav","start":0,"len":64,"warp_ratio":0.5,"pitch_semitones":24.0}]}]"#;
        let document = ProjectDocument::from_layout_json("Demo", 120.0, 48_000.0, valid).unwrap();
        assert_eq!(document.regions[0].warp_ratio, 0.5);
        assert_eq!(document.regions[0].pitch_semitones, 24.0);
        assert_eq!(document.regions[0].loop_count, 1);

        let looped = r#"[{"id":1,"name":"Audio","regions":[{"id":1,"path":"take.wav","start":0,"len":64,"loop_count":1024}]}]"#;
        let document = ProjectDocument::from_layout_json("Demo", 120.0, 48_000.0, looped).unwrap();
        assert_eq!(document.regions[0].loop_count, 1024);

        let invalid_loop = r#"[{"id":1,"name":"Audio","regions":[{"id":1,"path":"take.wav","start":0,"len":64,"loop_count":1025}]}]"#;
        assert!(ProjectDocument::from_layout_json("Demo", 120.0, 48_000.0, invalid_loop).is_err());
    }

    #[test]
    fn sandbox_plugin_metadata_round_trips_without_losing_state() {
        let layout = r#"[{"id":1,"name":"Vocal","plugin_types":[4294967295],"plugin_state_hex":["00a1ff"],"sandbox_plugin_paths":["/tmp/test.clap"],"sandbox_plugin_state_hex":["00a1ff"],"regions":[]}]"#;
        let document =
            ProjectDocument::from_layout_json("Sandbox", 120.0, 48_000.0, layout).unwrap();
        assert_eq!(document.tracks[0].plugin_types, vec![u32::MAX]);
        assert_eq!(
            document.tracks[0].sandbox_plugin_paths,
            vec!["/tmp/test.clap"]
        );
        assert_eq!(
            document.tracks[0].sandbox_plugin_states,
            vec![vec![0x00, 0xa1, 0xff]]
        );
        assert_eq!(document.plugin_instances.len(), 1);
        assert_eq!(document.plugin_instances[0].track_id, 1);
        assert_eq!(document.plugin_instances[0].format, PluginFormat::Clap);
        assert_eq!(document.plugin_instances[0].capability, "sandbox");
        let path = temp_path("json");
        document.save_atomic(path.to_str().unwrap()).unwrap();
        let loaded = ProjectDocument::load(path.to_str().unwrap()).unwrap();
        assert_eq!(loaded, document);
        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn unknown_plugin_state_versions_fail_closed() {
        let layout = r#"[{"id":1,"name":"Vocal","plugin_types":[0],"plugin_state_hex":["00a1ff"],"plugin_state_versions":[2],"regions":[]}]"#;
        let error = ProjectDocument::from_layout_json("Versioned", 120.0, 48_000.0, layout)
            .expect_err("unknown plugin state versions must not be silently restored");
        assert!(error
            .to_string()
            .contains("unsupported plugin state version"));

        let legacy_layout = r#"[{"id":1,"name":"Vocal","plugin_types":[0],"plugin_state_hex":["00a1ff"],"regions":[]}]"#;
        let document = ProjectDocument::from_layout_json("Legacy", 120.0, 48_000.0, legacy_layout)
            .expect("legacy state without a version uses the current version");
        assert_eq!(
            document.tracks[0].plugin_state_versions,
            vec![PLUGIN_STATE_SCHEMA_VERSION]
        );

        let version_zero = r#"[{"id":1,"name":"Legacy","plugin_types":[0],"plugin_state_hex":["00a1ff"],"plugin_state_versions":[0],"regions":[]}]"#;
        let migrated = ProjectDocument::from_layout_json("Migrated", 120.0, 48_000.0, version_zero)
            .expect("version zero raw state should migrate to current metadata");
        assert_eq!(
            migrated.tracks[0].plugin_state_versions,
            vec![PLUGIN_STATE_SCHEMA_VERSION]
        );
    }

    #[test]
    fn malformed_or_inconsistent_projects_are_rejected() {
        assert!(ProjectDocument::from_layout_json("Demo", 128.0, 48_000.0, "not-json").is_err());
        let mut document = ProjectDocument {
            schema_version: PROJECT_SCHEMA_VERSION,
            contract_version: PROJECT_CONTRACT_VERSION,
            project_id: new_project_id(),
            metadata: ProjectMetadata {
                name: "Demo".into(),
                version: PROJECT_SCHEMA_VERSION,
                bpm: 128.0,
                tracks_count: 1,
                key_root: 0,
                scale_type: 0,
            },
            sample_rate: 48_000.0,
            master_gain: 1.0,
            cycle_start_sample: 0,
            cycle_end_sample: 0,
            cycle_enabled: false,
            metronome_enabled: false,
            aux_track_ids: Vec::new(),
            comp_takes: Vec::new(),
            comp_segments: Vec::new(),
            tracks: vec![ProjectTrack {
                id: 1,
                name: "Track".into(),
                track_type: "Audio".into(),
                volume: 1.0,
                pan: 0.0,
                muted: false,
                solo: false,
                record_armed: false,
                phase_invert: false,
                track_delay_samples: 0,
                volume_automation: Vec::new(),
                pan_automation: Vec::new(),
                track_delay_automation: Vec::new(),
                plugin_types: Vec::new(),
                plugin_bypasses: Vec::new(),
                plugin_parameter_values: Vec::new(),
                plugin_states: Vec::new(),
                plugin_gui_states: Vec::new(),
                plugin_state_versions: Vec::new(),
                sandbox_plugin_paths: Vec::new(),
                sandbox_plugin_states: Vec::new(),
                sandbox_plugin_state_versions: Vec::new(),
            }],
            regions: Vec::new(),
            plugin_instances: Vec::new(),
            midi_learn_mappings: Vec::new(),
            midi_notes: Vec::new(),
            chord_track: Vec::new(),
            midi_events: Vec::new(),
            tempo_events: Vec::new(),
            time_signature_events: Vec::new(),
            macro_mappings: Vec::new(),
            warp_markers: Vec::new(),
            render_targets: Vec::new(),
            freeze_artifacts: Vec::new(),
            sidechain_routes: Vec::new(),
            feedback_routes: Vec::new(),
            audio_routes: Vec::new(),
            openutau_vocals: Vec::new(),
            track_stacks: Vec::new(),
            markers: Vec::new(),
            vca_groups: Vec::new(),
            hardware_inserts: Vec::new(),
        };
        document.metadata.tracks_count = 2;
        assert!(document.validate().is_err());
    }

    #[test]
    fn truncated_project_layout_prefixes_never_hydrate_successfully() {
        let valid = r#"[{"id":1,"name":"Track","track_type":"Audio","volume":1.0,"pan":0.0,"regions":[{"id":2,"path":"a.wav","start":0,"len":64}]}]"#;
        for end in 0..valid.len() {
            let prefix = &valid[..end];
            assert!(
                ProjectDocument::from_layout_json("Truncated", 120.0, 48_000.0, prefix).is_err(),
                "truncated prefix at {end} must be rejected"
            );
        }
    }

    #[test]
    fn mutated_project_layout_corpus_never_panics_or_accepts_invalid_state() {
        let valid = br#"[{"id":1,"name":"Track","track_type":"Audio","volume":1.0,"pan":0.0,"regions":[{"id":2,"path":"a.wav","start":0,"len":64}]}]"#;
        for index in 0..valid.len() {
            let mut mutated = valid.to_vec();
            mutated[index] ^= 0xff;
            let result = std::panic::catch_unwind(|| {
                ProjectDocument::from_layout_json(
                    "Mutated",
                    120.0,
                    48_000.0,
                    std::str::from_utf8(&mutated).unwrap_or("<invalid utf8>"),
                )
            });
            assert!(result.is_ok(), "mutation at {index} must not panic");
            if let Ok(Ok(document)) = result {
                assert!(
                    document.validate().is_ok(),
                    "accepted mutation at {index} must validate"
                );
            }
        }

        let unknown_fields = r#"[{"id":1,"name":"Track","track_type":"Audio","volume":1.0,"pan":0.0,"future_field":{"nested":true},"regions":[]}]"#;
        assert!(
            ProjectDocument::from_layout_json("Unknown", 120.0, 48_000.0, unknown_fields).is_ok()
        );
    }

    #[test]
    fn region_ids_are_unique_across_tracks() {
        let layout = r#"[
            {"id":1,"name":"A","regions":[{"id":7,"path":"a.wav","start":0,"len":64}]},
            {"id":2,"name":"B","regions":[{"id":7,"path":"b.wav","start":0,"len":64}]}
        ]"#;
        assert!(ProjectDocument::from_layout_json("Demo", 120.0, 48_000.0, layout).is_err());
    }

    #[test]
    fn unsupported_schema_is_rejected_on_load() {
        let path = temp_path("json");
        std::fs::write(&path, br#"{"schema_version":99}"#).unwrap();
        let result = ProjectDocument::load(path.to_str().unwrap());
        assert!(result.is_err());
        let _ = std::fs::remove_file(path);
    }

    #[test]
    fn invalid_native_fields_are_rejected_before_hydration() {
        let mut document = ProjectDocument {
            schema_version: PROJECT_SCHEMA_VERSION,
            contract_version: PROJECT_CONTRACT_VERSION,
            project_id: new_project_id(),
            metadata: ProjectMetadata {
                name: "Demo".into(),
                version: PROJECT_SCHEMA_VERSION,
                bpm: 128.0,
                tracks_count: 1,
                key_root: 0,
                scale_type: 0,
            },
            sample_rate: 48_000.0,
            master_gain: 1.0,
            cycle_start_sample: 0,
            cycle_end_sample: 0,
            cycle_enabled: false,
            metronome_enabled: false,
            aux_track_ids: Vec::new(),
            comp_takes: Vec::new(),
            comp_segments: Vec::new(),
            tracks: vec![ProjectTrack {
                id: 1,
                name: "Track".into(),
                track_type: "Unknown".into(),
                volume: 1.0,
                pan: 0.0,
                muted: false,
                solo: false,
                record_armed: false,
                phase_invert: false,
                track_delay_samples: 0,
                volume_automation: Vec::new(),
                pan_automation: Vec::new(),
                track_delay_automation: Vec::new(),
                plugin_types: Vec::new(),
                plugin_bypasses: Vec::new(),
                plugin_parameter_values: Vec::new(),
                plugin_states: Vec::new(),
                plugin_gui_states: Vec::new(),
                plugin_state_versions: Vec::new(),
                sandbox_plugin_paths: Vec::new(),
                sandbox_plugin_states: Vec::new(),
                sandbox_plugin_state_versions: Vec::new(),
            }],
            regions: Vec::new(),
            plugin_instances: Vec::new(),
            midi_learn_mappings: Vec::new(),
            midi_notes: Vec::new(),
            chord_track: Vec::new(),
            midi_events: Vec::new(),
            tempo_events: Vec::new(),
            time_signature_events: Vec::new(),
            macro_mappings: Vec::new(),
            warp_markers: Vec::new(),
            render_targets: Vec::new(),
            freeze_artifacts: Vec::new(),
            sidechain_routes: Vec::new(),
            feedback_routes: Vec::new(),
            audio_routes: Vec::new(),
            openutau_vocals: Vec::new(),
            track_stacks: Vec::new(),
            markers: Vec::new(),
            vca_groups: Vec::new(),
            hardware_inserts: Vec::new(),
        };
        assert!(document.validate().is_err());

        document.tracks[0].track_type = "Audio".into();
        document.tracks[0].pan = 2.0;
        assert!(document.validate().is_err());

        document.tracks[0].pan = 0.0;
        document.sample_rate = 48_000.5;
        assert!(document.validate().is_err());
    }

    #[test]
    fn native_track_type_mapping_is_explicit() {
        assert_eq!(ProjectDocument::native_track_type_code("Audio").unwrap(), 0);
        assert_eq!(ProjectDocument::native_track_type_code("Midi").unwrap(), 1);
        assert_eq!(
            ProjectDocument::native_track_type_code("Instrument").unwrap(),
            2
        );
        assert_eq!(ProjectDocument::native_track_type_code("Bus").unwrap(), 3);
        assert_eq!(ProjectDocument::native_track_type_code("Aux").unwrap(), 3);
        assert_eq!(ProjectDocument::native_track_type_code("Vocal").unwrap(), 4);
        assert!(ProjectDocument::native_track_type_code("audio").is_err());
    }

    #[test]
    fn project_identity_is_stable_across_serialization() {
        let document = ProjectDocument::from_layout_json("Identity", 120.0, 48_000.0, "[]").unwrap();
        assert!(Uuid::parse_str(&document.project_id).is_ok());
        let bytes = serde_json::to_vec(&document).unwrap();
        let restored: ProjectDocument = serde_json::from_slice(&bytes).unwrap();
        assert_eq!(restored.project_id, document.project_id);
        let other = ProjectDocument::from_layout_json("Identity", 120.0, 48_000.0, "[]").unwrap();
        assert_ne!(other.project_id, document.project_id);
    }

    #[test]
    fn freeze_artifact_metadata_round_trips_with_project() {
        let mut document = ProjectDocument::from_layout_json(
            "Freeze", 120.0, 48_000.0,
            r#"[{"id":1,"name":"Synth","track_type":"Instrument","volume":1.0,"pan":0.0,"regions":[]}]"#,
        ).unwrap();
        document.freeze_artifacts.push(FreezeArtifactContract {
            track_id: 1,
            project_generation: 4,
            audio_generation: 8,
            total_samples: 48_000,
            sample_rate: 48_000,
            path: "freeze/track-1.wav".into(),
            content_checksum: 123,
        });
        document.validate().unwrap();
        let restored: ProjectDocument = serde_json::from_slice(
            &serde_json::to_vec(&document).unwrap(),
        ).unwrap();
        assert_eq!(restored.freeze_artifacts, document.freeze_artifacts);
    }

    #[test]
    fn freeze_artifact_must_reference_an_existing_track() {
        let mut document = ProjectDocument::from_layout_json("Freeze", 120.0, 48_000.0, "[]").unwrap();
        document.freeze_artifacts.push(FreezeArtifactContract {
            track_id: 9,
            project_generation: 1,
            audio_generation: 1,
            total_samples: 1,
            sample_rate: 48_000,
            path: "freeze.wav".into(),
            content_checksum: 1,
        });
        assert!(document.validate().is_err());
    }

    #[test]
    fn aggregate_project_limits_reject_resource_exhaustion_inputs() {
        let mut document = ProjectDocument::from_layout_json("Limits", 120.0, 48_000.0, "[]").unwrap();
        document.tracks = (1..=(MAX_PROJECT_TRACKS as u32 + 1))
            .map(|id| ProjectTrack {
                id,
                name: format!("Track {id}"),
                track_type: "Audio".into(),
                volume: 1.0,
                pan: 0.0,
                muted: false,
                solo: false,
                record_armed: false,
                phase_invert: false,
                track_delay_samples: 0,
                volume_automation: Vec::new(),
                pan_automation: Vec::new(),
                track_delay_automation: Vec::new(),
                plugin_types: Vec::new(),
                plugin_bypasses: Vec::new(),
                plugin_parameter_values: Vec::new(),
                plugin_states: Vec::new(),
                plugin_gui_states: Vec::new(),
                plugin_state_versions: Vec::new(),
                sandbox_plugin_paths: Vec::new(),
                sandbox_plugin_states: Vec::new(),
                sandbox_plugin_state_versions: Vec::new(),
            })
            .collect();
        document.metadata.tracks_count = document.tracks.len() as u32;
        assert!(document.validate().is_err());

    }

    #[test]
    fn scheduled_midi_notes_roundtrip_and_validate_against_tracks() {
        let mut document = ProjectDocument::from_layout_json(
            "MIDI", 120.0, 48_000.0,
            r#"[{"id":1,"name":"Keys","track_type":"Midi","volume":1.0,"pan":0.0,"regions":[]}]"#,
        ).unwrap();
        document.midi_notes.push(MidiNoteContract {
            track_id: 1, pitch: 60, velocity: 100, start_sample: 48_000,
            length_samples: 24_000,
            lyric: "la".into(),
            phoneme: "la".into(), pitch_curve_cents: vec![0, 14, -8],
            vibrato_depth_cents: 28, portamento_samples: 1200,
            probability: 100, repeat_count: 1,
        });
        document.midi_events = vec![
            crate::midi::MIDIEvent {
                beat: 1.5,
                channel: 0,
                kind: crate::midi::MIDIEventKind::ControlChange {
                    controller: 74,
                    value: 96,
                },
            },
            crate::midi::MIDIEvent {
                beat: 2.0,
                channel: 0,
                kind: crate::midi::MIDIEventKind::SysEx {
                    data: vec![0x7d, 0x01, 0x02],
                },
            },
            crate::midi::MIDIEvent {
                beat: 2.5,
                channel: 1,
                kind: crate::midi::MIDIEventKind::Midi2ChannelVoice {
                    status: 0x9,
                    index: 60,
                    value: 0x8000_0000,
                },
            },
        ];
        document.validate().unwrap();
        let restored: ProjectDocument = serde_json::from_slice(
            &serde_json::to_vec(&document).unwrap(),
        ).unwrap();
        assert_eq!(restored.midi_notes, document.midi_notes);
        assert_eq!(restored.midi_events, document.midi_events);
        document.midi_notes[0].portamento_samples = 24_001;
        assert!(document.validate().is_err());
        document.midi_notes[0].portamento_samples = 1_200;
        document.midi_notes[0].track_id = 99;
        assert!(document.validate().is_err());
    }


    #[test]
    fn reproducibility_manifest_is_deterministic_and_tracks_content_changes() {
        let mut document = ProjectDocument::from_layout_json(
            "Manifest", 120.0, 48_000.0,
            r#"[{"id":1,"name":"Vocal","track_type":"Audio","volume":1.0,"pan":0.0,"regions":[]}]"#,
        ).unwrap();
        let first = document.reproducibility_manifest().unwrap();
        let round_tripped: ProjectDocument = serde_json::from_slice(
            &serde_json::to_vec(&document).unwrap(),
        ).unwrap();
        let second = round_tripped.reproducibility_manifest().unwrap();
        assert_eq!(first["snapshot_sha256"], second["snapshot_sha256"]);
        assert_eq!(first["asset_reference_sha256"], second["asset_reference_sha256"]);
        document.master_gain = 0.75;
        let changed = document.reproducibility_manifest().unwrap();
        assert_ne!(first["snapshot_sha256"], changed["snapshot_sha256"]);
        assert_eq!(first["asset_reference_sha256"], changed["asset_reference_sha256"]);
    }
}
