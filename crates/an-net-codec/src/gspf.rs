use std::cmp::min;
use std::collections::HashSet;
use std::num::NonZeroUsize;
use std::ops::Range;
use lru::LruCache;
use tokio_util::bytes::BytesMut;
use serde::{Deserialize, Serialize};


#[derive(
    Clone, Copy, Debug, PartialOrd, PartialEq,
    Serialize, Deserialize
)]
#[repr(u8)]
pub enum GspfImageFormat {
    UNKNOWN = 0,
    STRING,
    MJPEG
}

pub(crate) const MTU: usize = 1472;

pub struct GspfPacketizer {
    mtu: usize,
    format: GspfImageFormat,
    width: u16,
    height: u16,
    frame: u16,
}

impl GspfPacketizer {
    pub fn new(format: GspfImageFormat, (width, height): (u16, u16), mtu: Option<usize>) -> Self {
        let mtu = mtu.unwrap_or(MTU);
        assert!(mtu > HEADER_SIZE);
        Self {
            mtu,
            format,
            width,
            height,
            frame: 0
        }
    }

    pub fn reset_frame_count(&mut self) {
        self.frame = 0;
    }

    pub fn reset_size(&mut self, (width, height): (u16, u16)) {
        self.width = width;
        self.height = height
    }

    pub fn new_packetizer<'a>(&mut self, data: &'a [u8]) -> GspfPacketizerIter<'a> {
        let total_packets = data.len().div_ceil(self.mtu - HEADER_SIZE) as u16;
        let total_bytes = data.len();
        let packetizer = GspfPacketizerIter {
            mtu: self.mtu,
            frame: self.frame,
            packet: 0,
            packets_in_frame: total_packets,
            format: self.format,
            bytes_sent: 0,
            total_bytes,
            width: self.width,
            height: self.height,
            data,
        };
        self.frame = self.frame.wrapping_add(1);
        packetizer
    }
}

pub struct GspfPacketizerIter<'a> {
    mtu: usize,
    frame: u16,
    packet: u16,
    packets_in_frame: u16,
    format: GspfImageFormat,
    bytes_sent: usize,
    total_bytes: usize,
    width: u16,
    height: u16,
    data: &'a [u8]
}

impl<'a> Iterator for GspfPacketizerIter<'a> {
    type Item = GspfSendPacket<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.packet >= self.packets_in_frame {
            return None
        }
        let packet_data_length = min(
            self.total_bytes - self.bytes_sent, self.mtu - HEADER_SIZE
        );
        let bytes_end = self.bytes_sent + packet_data_length;
        let next_packet = Self::Item {
            header: GspfHeader {
                frame: self.frame,
                frame_size: self.total_bytes as u32,
                packet: self.packet,
                packets_in_frame: self.packets_in_frame,
                format: self.format,
                width: self.width,
                height: self.height,
                size: packet_data_length as u16
            },
            data: &self.data[self.bytes_sent..bytes_end]
        };
        self.bytes_sent = bytes_end;
        self.packet += 1;
        Some(next_packet)
    }
}

// 4 * 1 + 2 * 6 + 1, but really 20 due to padding
pub(crate) const HEADER_SIZE: usize = 20;

#[derive(Serialize, Deserialize, Debug)]
pub struct GspfHeader {
    pub frame: u16,
    pub frame_size: u32,
    pub packet: u16,
    pub packets_in_frame: u16,
    pub width: u16,
    pub height: u16,
    pub size: u16,
    pub format: GspfImageFormat,
}

pub struct GspfSendPacket<'a> {
    pub header: GspfHeader,
    pub data: &'a [u8]
}

pub struct GspfDepacketizer {
    mtu: usize,
    height: u16,
    width: u16,
    format: GspfImageFormat,
    max_frame_size: usize,
    // really we just need a set here; will probably contribute to crate
    recent_frames: LruCache<u16, u16>,
    current_frame: Option<GspfDepacketizerFrame>,
    packets: HashSet<u16>,
}

impl GspfDepacketizer {

    pub fn new(
        format: GspfImageFormat, (height, width): (u16, u16), max_frame_size: usize,
        mtu: Option<usize>
    ) -> Self {
        let mtu = mtu.unwrap_or(MTU);
        assert!(mtu > HEADER_SIZE);
        Self {
            mtu,
            height,
            width,
            format,
            max_frame_size,
            recent_frames: LruCache::new(NonZeroUsize::new(u8::MAX as usize).unwrap()),
            current_frame: None,
            packets: HashSet::new(),
        }
    }

    pub fn receive_packet_header(&mut self, header: &GspfHeader) -> GspfRecieveType {
        let current_frame = self.current_frame.as_ref();
        if current_frame.is_none() ||
            current_frame.unwrap().frame != header.frame
        {
            return if let Some(setup_err) = self.try_setup_new_frame(header) {
                setup_err
            } else {
                GspfRecieveType::NewFrame(self.take_packet(header))
            }
        }
        // we could check MTU size real quick, but leave that to the network codec
        if self.packets.contains(&header.packet) {
            // we're decoding a packet we've already seen
            return GspfRecieveType::DuplicatePacket;
        }
        let current_frame = current_frame.unwrap();
        // ensure we EXPECT the packet
        if header.packet >= current_frame.packets as u16 {
            return GspfRecieveType::BadPacket;
        }
        let next_size = current_frame.bytes_received + header.size as u32;
        // ensure we aren't overflowing
        if next_size > current_frame.size {
            return GspfRecieveType::BadFrame;
        }
        // if we are on the final packet
        return if (self.packets.len() + 1) == current_frame.packets {
            // ensure that the size matches
            if next_size != current_frame.size {
                self.current_frame.take();
                GspfRecieveType::CorruptedFrame
            } else {
                // remove current frame so next iter we will get a new one, return final
                let final_range = self.take_packet(header);
                self.current_frame.take();
                GspfRecieveType::FinishedFrame(final_range)
            }
        } else {
            GspfRecieveType::CurrentFrame(self.take_packet(header))
        }
    }

    fn try_setup_new_frame(&mut self, header: &GspfHeader) -> Option<GspfRecieveType> {
        if self.recent_frames.contains(&header.frame) {
            return Some(GspfRecieveType::OldFrame);
        }
        self.recent_frames.push(header.frame, header.frame);
        // only check the frame the first go around; otherwise, trust following packets while parsing
        if header.height != self.height ||
            header.width != self.width ||
            header.format != self.format ||
            header.frame_size as usize > self.max_frame_size
        {
            return Some(GspfRecieveType::BadFrame);
        }
        self.packets.clear();
        self.packets.reserve(header.packets_in_frame as usize);
        self.current_frame = Some(GspfDepacketizerFrame {
            frame: header.frame,
            size: header.frame_size,
            bytes_received: 0,
            packets: header.packets_in_frame as usize,
        });
        None
    }

    fn take_packet(&mut self, header: &GspfHeader) -> Range<usize> {
        let current_frame = self.current_frame.as_mut().unwrap();
        self.packets.insert(header.packet);
        current_frame.bytes_received += header.size as u32;
        // assume all packets are full until the last one
        let start = (header.packet as usize) * (self.mtu - HEADER_SIZE);
        let end = start + header.size as usize;
        start..end
    }
}

struct GspfDepacketizerFrame {
    frame: u16,
    size: u32,
    bytes_received: u32,
    packets: usize,
}

pub struct GspfReceivePacket {
    pub header: GspfHeader,
    pub data: BytesMut
}

#[derive(PartialEq, Debug, Clone)]
pub enum GspfRecieveType {
    NewFrame(Range<usize>),
    CurrentFrame(Range<usize>),
    FinishedFrame(Range<usize>),
    DuplicatePacket,
    BadPacket,
    OldFrame,
    CorruptedFrame,
    BadFrame,
}

// not going to use it, but keeping around because its neat
#[inline(always)]
fn rolling_less_than(a: &u16, b: &u16) -> bool {
    // some grace thrown in so we don't get the wrong answer if a BUNCH of packets were skipped
    static ROLLOVER: u16 = 2u16.pow(16 - 1);
    return a != b && (
        (a < b && (b - a) < ROLLOVER) ||
        (a > b && (a - b) > ROLLOVER)
    )
}

#[cfg(test)]
mod tests {
    use crate::gspf::{
        GspfImageFormat, GspfPacketizer, HEADER_SIZE, GspfHeader, GspfDepacketizer, rolling_less_than
    };
    use crate::gspf::GspfRecieveType::{
        NewFrame, CurrentFrame, FinishedFrame,
        OldFrame, BadFrame, CorruptedFrame,
        BadPacket, DuplicatePacket
    };

    #[test]
    fn gspf_header_packing() {
        let header_small = GspfHeader {
            frame: 0,
            frame_size: 0,
            packet: 0,
            packets_in_frame: 0,
            width: 0,
            height: 0,
            size: 0,
            format: GspfImageFormat::UNKNOWN,
        };
        let buffer_small = bincode::serialize(&header_small).unwrap();
        let header_large = GspfHeader {
            frame: u16::MAX,
            frame_size: u32::MAX,
            packet: u16::MAX,
            packets_in_frame: u16::MAX,
            width: u16::MAX,
            height: u16::MAX,
            size: u16::MAX,
            format: GspfImageFormat::MJPEG,
        };
        let buffer_large = bincode::serialize(&header_small).unwrap();
        assert_eq!(buffer_small.len(), buffer_large.len());
        assert_eq!(buffer_large.len(), HEADER_SIZE);
    }

    #[test]
    fn gspf_packetize_iterates() {
        let mut packetizer = GspfPacketizer::new(
            GspfImageFormat::STRING, (1, 1), Some(HEADER_SIZE + 10)
        );
        let data: Vec<u8> = (0..35).collect();
        let mut iter = packetizer.new_packetizer(&data);
        assert!(iter.next().is_some());
        assert!(iter.next().is_some());
        assert!(iter.next().is_some());
        assert!(iter.next().is_some());
        assert!(iter.next().is_none());
    }

    #[test]
    fn gspf_packetize_is_reconstructible() {
        let mut packetizer = GspfPacketizer::new(
            GspfImageFormat::STRING, (1, 1), Some(HEADER_SIZE + 10)
        );
        let data: Vec<u8> = (0..35).collect();
        let mut output: Vec<Vec<u8>> = Vec::new();
        for packet in packetizer.new_packetizer(&data) {
            if output.len() == 0 {
                output = vec![Vec::new(); packet.header.packets_in_frame as usize]
            }
            output[packet.header.packet as usize] = packet.data.to_vec();
        }
        assert_eq!(data, output.into_iter().flatten().collect::<Vec<u8>>());
    }

    #[test]
    fn gspf_depacketize_throws_out_garbage() {
        let mut depacketizer = GspfDepacketizer::new(
            GspfImageFormat::STRING, (1, 1), 100, Some(HEADER_SIZE + 10)
        );

        let frame_wrong_format = GspfHeader {
            frame: 0,
            frame_size: 200,
            packet: 0,
            packets_in_frame: 3,
            width: 4,
            height: 4,
            size: 90,
            format: GspfImageFormat::STRING,
        };
        assert_eq!(depacketizer.receive_packet_header(&frame_wrong_format), BadFrame);
        assert_eq!(depacketizer.receive_packet_header(&frame_wrong_format), OldFrame);

        let mut good_frame = GspfHeader {
            frame: 1,
            frame_size: 25,
            packet: 0,
            packets_in_frame: 3,
            width: 1,
            height: 1,
            size: 10,
            format: GspfImageFormat::STRING,
        };
        assert_eq!(depacketizer.receive_packet_header(&good_frame), NewFrame(0..10));

        good_frame.frame = 2;
        good_frame.packet = 1;
        assert_eq!(depacketizer.receive_packet_header(&good_frame), NewFrame(10..20));

        good_frame.packet = 0;
        assert_eq!(depacketizer.receive_packet_header(&good_frame), CurrentFrame(0..10));

        good_frame.packet = 3;
        good_frame.size = 5;
        assert_eq!(depacketizer.receive_packet_header(&good_frame), BadPacket);

        good_frame.packet = 2;
        assert_eq!(depacketizer.receive_packet_header(&good_frame), FinishedFrame(20..25));

        assert_eq!(depacketizer.receive_packet_header(&good_frame), OldFrame);
    }

    #[test]
    fn gspf_depacketize_packetized_packets() {
        let mut depacketizer = GspfDepacketizer::new(
            GspfImageFormat::STRING, (1, 1), 50000, Some(HEADER_SIZE + 500)
        );
        let mut packetizer = GspfPacketizer::new(
            GspfImageFormat::STRING, (1, 1), Some(HEADER_SIZE + 500)
        );
        let data: Vec<u8> = vec![8u8; 38702];
        for _ in 0..1000 {
            let mut it = packetizer.new_packetizer(&data).enumerate().peekable();
            while let Some((i, packet)) = it.next() {
                let start = i * 500;
                let end = start + packet.header.size as usize;
                if i == 0 {
                    assert_eq!(
                        depacketizer.receive_packet_header(&packet.header), NewFrame(start..end)
                    );
                } else if it.peek().is_some() {
                    assert_eq!(
                        depacketizer.receive_packet_header(&packet.header), CurrentFrame(start..end)
                    );
                } else {
                    assert_eq!(
                        depacketizer.receive_packet_header(&packet.header), FinishedFrame(start..end)
                    );
                }
            }
        }

    }

    #[test]
    fn rollover() {
        // obvious case
        assert!(rolling_less_than(&10u16, &50u16));

        // not obvious
        assert!(rolling_less_than(&u16::MAX, &0u16));

        // kinda scuffed
        assert!(rolling_less_than(&(u16::MAX - 10u16), &5u16));

        // negate obvious
        assert!(!rolling_less_than(&5000u16, &70u16));

        // negate not obvious
        assert!(!rolling_less_than(&20u16, &(u16::MAX - 5u16)));

        // negate scuffed
        assert!(!rolling_less_than(&200u16, &(u16::MAX - 100u16)));
    }
}
