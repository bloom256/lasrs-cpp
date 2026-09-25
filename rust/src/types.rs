// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::error::BoxError;
use las::{
    Bounds, Color, GpsTimeType, Point, Transform, Vector, Version, Vlr,
    copc::{Entry, VoxelKey},
    point::{Classification, Format, ScanDirection},
    raw::point::Waveform,
};
use std::slice;

/// Borrowed UTF-8 string, not NUL-terminated.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsStr {
    pub ptr: *const u8,
    pub len: usize,
}

impl LasrsStr {
    pub(crate) fn new(s: &str) -> Self {
        Self {
            ptr: s.as_ptr(),
            len: s.len(),
        }
    }

    /// # Safety
    /// `ptr` and `len` must describe memory that stays valid for `'a`.
    pub(crate) unsafe fn to_str<'a>(self) -> Result<&'a str, BoxError> {
        Ok(std::str::from_utf8(unsafe {
            borrow_slice(self.ptr, self.len)
        })?)
    }
}

/// Borrowed byte slice.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsBytes {
    pub ptr: *const u8,
    pub len: usize,
}

impl LasrsBytes {
    pub(crate) fn new(bytes: &[u8]) -> Self {
        Self {
            ptr: bytes.as_ptr(),
            len: bytes.len(),
        }
    }

    /// # Safety
    /// `ptr` and `len` must describe memory that stays valid for `'a`.
    pub(crate) unsafe fn as_slice<'a>(self) -> &'a [u8] {
        unsafe { borrow_slice(self.ptr, self.len) }
    }
}

/// # Safety
/// If `len > 0`, `ptr` must point to `len` readable elements valid for `'a`.
pub(crate) unsafe fn borrow_slice<'a, T>(ptr: *const T, len: usize) -> &'a [T] {
    if len == 0 {
        &[]
    } else {
        unsafe { slice::from_raw_parts(ptr, len) }
    }
}

/// # Safety
/// If `len > 0`, `ptr` must point to `len` writable elements valid for `'a`.
pub(crate) unsafe fn borrow_slice_mut<'a, T>(ptr: *mut T, len: usize) -> &'a mut [T] {
    if len == 0 {
        &mut []
    } else {
        unsafe { slice::from_raw_parts_mut(ptr, len) }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsVersion {
    pub major: u8,
    pub minor: u8,
}

impl From<Version> for LasrsVersion {
    fn from(v: Version) -> Self {
        Self {
            major: v.major,
            minor: v.minor,
        }
    }
}

impl From<LasrsVersion> for Version {
    fn from(v: LasrsVersion) -> Self {
        Version::new(v.major, v.minor)
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsTransform {
    pub scale: f64,
    pub offset: f64,
}

impl From<Transform> for LasrsTransform {
    fn from(t: Transform) -> Self {
        Self {
            scale: t.scale,
            offset: t.offset,
        }
    }
}

impl From<LasrsTransform> for Transform {
    fn from(t: LasrsTransform) -> Self {
        Transform {
            scale: t.scale,
            offset: t.offset,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsTransforms {
    pub x: LasrsTransform,
    pub y: LasrsTransform,
    pub z: LasrsTransform,
}

impl From<Vector<Transform>> for LasrsTransforms {
    fn from(t: Vector<Transform>) -> Self {
        Self {
            x: t.x.into(),
            y: t.y.into(),
            z: t.z.into(),
        }
    }
}

impl From<LasrsTransforms> for Vector<Transform> {
    fn from(t: LasrsTransforms) -> Self {
        Vector {
            x: t.x.into(),
            y: t.y.into(),
            z: t.z.into(),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsVector {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

impl From<Vector<f64>> for LasrsVector {
    fn from(v: Vector<f64>) -> Self {
        Self {
            x: v.x,
            y: v.y,
            z: v.z,
        }
    }
}

impl From<LasrsVector> for Vector<f64> {
    fn from(v: LasrsVector) -> Self {
        Vector {
            x: v.x,
            y: v.y,
            z: v.z,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsBounds {
    pub min: LasrsVector,
    pub max: LasrsVector,
}

impl From<Bounds> for LasrsBounds {
    fn from(b: Bounds) -> Self {
        Self {
            min: b.min.into(),
            max: b.max.into(),
        }
    }
}

impl From<LasrsBounds> for Bounds {
    fn from(b: LasrsBounds) -> Self {
        Bounds {
            min: b.min.into(),
            max: b.max.into(),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsFormat {
    pub has_gps_time: bool,
    pub has_color: bool,
    pub is_extended: bool,
    pub has_waveform: bool,
    pub has_nir: bool,
    pub extra_bytes: u16,
    pub is_compressed: bool,
}

impl From<Format> for LasrsFormat {
    fn from(f: Format) -> Self {
        Self {
            has_gps_time: f.has_gps_time,
            has_color: f.has_color,
            is_extended: f.is_extended,
            has_waveform: f.has_waveform,
            has_nir: f.has_nir,
            extra_bytes: f.extra_bytes,
            is_compressed: f.is_compressed,
        }
    }
}

impl From<LasrsFormat> for Format {
    fn from(f: LasrsFormat) -> Self {
        Format {
            has_gps_time: f.has_gps_time,
            has_color: f.has_color,
            is_extended: f.is_extended,
            has_waveform: f.has_waveform,
            has_nir: f.has_nir,
            extra_bytes: f.extra_bytes,
            is_compressed: f.is_compressed,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct LasrsColor {
    pub red: u16,
    pub green: u16,
    pub blue: u16,
}

impl From<Color> for LasrsColor {
    fn from(c: Color) -> Self {
        Self {
            red: c.red,
            green: c.green,
            blue: c.blue,
        }
    }
}

impl From<LasrsColor> for Color {
    fn from(c: LasrsColor) -> Self {
        Color::new(c.red, c.green, c.blue)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct LasrsWaveform {
    pub wave_packet_descriptor_index: u8,
    pub byte_offset_to_waveform_data: u64,
    pub waveform_packet_size_in_bytes: u32,
    pub return_point_waveform_location: f32,
    pub x_t: f32,
    pub y_t: f32,
    pub z_t: f32,
}

impl From<Waveform> for LasrsWaveform {
    fn from(w: Waveform) -> Self {
        Self {
            wave_packet_descriptor_index: w.wave_packet_descriptor_index,
            byte_offset_to_waveform_data: w.byte_offset_to_waveform_data,
            waveform_packet_size_in_bytes: w.waveform_packet_size_in_bytes,
            return_point_waveform_location: w.return_point_waveform_location,
            x_t: w.x_t,
            y_t: w.y_t,
            z_t: w.z_t,
        }
    }
}

impl From<LasrsWaveform> for Waveform {
    fn from(w: LasrsWaveform) -> Self {
        Waveform {
            wave_packet_descriptor_index: w.wave_packet_descriptor_index,
            byte_offset_to_waveform_data: w.byte_offset_to_waveform_data,
            waveform_packet_size_in_bytes: w.waveform_packet_size_in_bytes,
            return_point_waveform_location: w.return_point_waveform_location,
            x_t: w.x_t,
            y_t: w.y_t,
            z_t: w.z_t,
        }
    }
}

pub(crate) fn gps_time_type_to_u8(t: GpsTimeType) -> u8 {
    u16::from(t) as u8
}

pub(crate) fn gps_time_type_from_u8(n: u8) -> GpsTimeType {
    GpsTimeType::from(u16::from(n))
}

/// A point; `extra_bytes` travel in a separate contiguous buffer, in point
/// order, `extra_bytes_len` bytes per point.
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct LasrsPoint {
    pub x: f64,
    pub y: f64,
    pub z: f64,
    pub intensity: u16,
    pub return_number: u8,
    pub number_of_returns: u8,
    /// 0 = RightToLeft, 1 = LeftToRight
    pub scan_direction: u8,
    pub is_edge_of_flight_line: bool,
    pub classification: u8,
    pub is_synthetic: bool,
    pub is_key_point: bool,
    pub is_withheld: bool,
    pub is_overlap: bool,
    pub scanner_channel: u8,
    pub scan_angle: f32,
    pub user_data: u8,
    pub point_source_id: u16,
    pub has_gps_time: bool,
    pub gps_time: f64,
    pub has_color: bool,
    pub color: LasrsColor,
    pub has_waveform: bool,
    pub waveform: LasrsWaveform,
    pub has_nir: bool,
    pub nir: u16,
    pub extra_bytes_len: usize,
}

impl From<&Point> for LasrsPoint {
    fn from(p: &Point) -> Self {
        Self {
            x: p.x,
            y: p.y,
            z: p.z,
            intensity: p.intensity,
            return_number: p.return_number,
            number_of_returns: p.number_of_returns,
            scan_direction: match p.scan_direction {
                ScanDirection::RightToLeft => 0,
                ScanDirection::LeftToRight => 1,
            },
            is_edge_of_flight_line: p.is_edge_of_flight_line,
            classification: p.classification.into(),
            is_synthetic: p.is_synthetic,
            is_key_point: p.is_key_point,
            is_withheld: p.is_withheld,
            is_overlap: p.is_overlap,
            scanner_channel: p.scanner_channel,
            scan_angle: p.scan_angle,
            user_data: p.user_data,
            point_source_id: p.point_source_id,
            has_gps_time: p.gps_time.is_some(),
            gps_time: p.gps_time.unwrap_or_default(),
            has_color: p.color.is_some(),
            color: p.color.map(Into::into).unwrap_or_default(),
            has_waveform: p.waveform.is_some(),
            waveform: p.waveform.map(Into::into).unwrap_or_default(),
            has_nir: p.nir.is_some(),
            nir: p.nir.unwrap_or_default(),
            extra_bytes_len: p.extra_bytes.len(),
        }
    }
}

impl LasrsPoint {
    pub(crate) fn to_point(self, extra_bytes: &[u8]) -> Result<Point, BoxError> {
        Ok(Point {
            x: self.x,
            y: self.y,
            z: self.z,
            intensity: self.intensity,
            return_number: self.return_number,
            number_of_returns: self.number_of_returns,
            scan_direction: match self.scan_direction {
                0 => ScanDirection::RightToLeft,
                1 => ScanDirection::LeftToRight,
                n => return Err(format!("invalid scan direction: {n}").into()),
            },
            is_edge_of_flight_line: self.is_edge_of_flight_line,
            classification: Classification::new(self.classification)?,
            is_synthetic: self.is_synthetic,
            is_key_point: self.is_key_point,
            is_withheld: self.is_withheld,
            is_overlap: self.is_overlap,
            scanner_channel: self.scanner_channel,
            scan_angle: self.scan_angle,
            user_data: self.user_data,
            point_source_id: self.point_source_id,
            gps_time: self.has_gps_time.then_some(self.gps_time),
            color: self.has_color.then(|| self.color.into()),
            waveform: self.has_waveform.then(|| self.waveform.into()),
            nir: self.has_nir.then_some(self.nir),
            extra_bytes: extra_bytes.to_vec(),
        })
    }
}

/// Converts `points` whose extra bytes are concatenated in `extra_bytes`.
///
/// # Safety
/// `points` must hold `n` points; `extra_bytes` must hold the sum of their
/// `extra_bytes_len`.
pub(crate) unsafe fn points_from_ffi(
    points: *const LasrsPoint,
    n: usize,
    extra_bytes: *const u8,
) -> Result<Vec<Point>, BoxError> {
    let points = unsafe { borrow_slice(points, n) };
    let total_extra = points.iter().map(|p| p.extra_bytes_len).sum();
    let mut extra = unsafe { borrow_slice(extra_bytes, total_extra) };
    points
        .iter()
        .map(|p| {
            let (mine, rest) = extra.split_at(p.extra_bytes_len);
            extra = rest;
            p.to_point(mine)
        })
        .collect()
}

/// Borrowed view of a [`Vlr`].
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsVlr {
    pub user_id: LasrsStr,
    pub record_id: u16,
    pub description: LasrsStr,
    pub data: LasrsBytes,
}

impl From<&Vlr> for LasrsVlr {
    fn from(v: &Vlr) -> Self {
        Self {
            user_id: LasrsStr::new(&v.user_id),
            record_id: v.record_id,
            description: LasrsStr::new(&v.description),
            data: LasrsBytes::new(&v.data),
        }
    }
}

impl LasrsVlr {
    /// # Safety
    /// All borrowed pointers must be valid.
    pub(crate) unsafe fn to_vlr(self) -> Result<Vlr, BoxError> {
        Ok(Vlr {
            user_id: unsafe { self.user_id.to_str() }?.to_string(),
            record_id: self.record_id,
            description: unsafe { self.description.to_str() }?.to_string(),
            data: unsafe { self.data.as_slice() }.to_vec(),
        })
    }
}

/// # Safety
/// `vlrs` must hold `n` valid views.
pub(crate) unsafe fn vlrs_from_ffi(vlrs: *const LasrsVlr, n: usize) -> Result<Vec<Vlr>, BoxError> {
    unsafe { borrow_slice(vlrs, n) }
        .iter()
        .map(|v| unsafe { v.to_vlr() })
        .collect()
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsVoxelKey {
    pub l: i32,
    pub x: i32,
    pub y: i32,
    pub z: i32,
}

impl From<VoxelKey> for LasrsVoxelKey {
    fn from(k: VoxelKey) -> Self {
        Self {
            l: k.l,
            x: k.x,
            y: k.y,
            z: k.z,
        }
    }
}

impl From<LasrsVoxelKey> for VoxelKey {
    fn from(k: LasrsVoxelKey) -> Self {
        VoxelKey {
            l: k.l,
            x: k.x,
            y: k.y,
            z: k.z,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsEntry {
    pub key: LasrsVoxelKey,
    pub offset: u64,
    pub byte_size: i32,
    pub point_count: i32,
}

impl From<Entry> for LasrsEntry {
    fn from(e: Entry) -> Self {
        Self {
            key: e.key.into(),
            offset: e.offset,
            byte_size: e.byte_size,
            point_count: e.point_count,
        }
    }
}

impl From<LasrsEntry> for Entry {
    fn from(e: LasrsEntry) -> Self {
        Entry {
            key: e.key.into(),
            offset: e.offset,
            byte_size: e.byte_size,
            point_count: e.point_count,
        }
    }
}
