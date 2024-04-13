use std::cmp::min;

// #[derive(Clone, Copy, Debug)]
// #[repr(u8)]
// pub enum GspfImageFormat {
//     UNKNOWN = 0,
//     STRING,
//     YUYV,
//     YUV420,
//     MJPEG
// }
//
// struct GspfPacketizerBuilder {
//
// }
//
// struct GspfPacketizer<'a, T> {
//     frame: u32,
//     total_frames: u32,
//     mtu: usize,
//     format: GspfImageFormat,
//     bytes_sent: usize,
//     total_bytes: usize,
//     data: &'a T
// }
//
//
// impl<'a, T> Iterator for GspfPacketizer<'a, T> {
//     type Item = GspfSendPacket<'a, T>;
//
//     fn next(&mut self) -> Option<Self::Item> {
//         if self.frame >= self.total_frames {
//             return None
//         }
//         let packet_data_length = min(
//             self.total_bytes - self.bytes_sent, self.mtu - HEADER_SIZE
//         );
//         let bytes_end = self.bytes_sent + packet_data_length;
//         let next_packet = Self::Item {
//             header: GspfHeader {
//                 total_frames: self.total_frames,
//                 frame: self.frame,
//                 format: self.format,
//                 size: packet_data_length as u32
//             },
//             data: self.data[self.bytes_sent..bytes_end]
//         };
//         self.bytes_sent = bytes_end;
//         self.frame += 1;
//         next_packet
//     }
//
// }
//
//
// const HEADER_SIZE: usize = 4 * 3 + 1;
//
// #[repr(packed(8))]
// struct GspfHeader {
//     total_frames: u32,
//     frame: u32,
//     format: GspfImageFormat,
//     size: u32,
// }
//
// struct GspfSendPacket<'a, T> {
//     header: GspfHeader,
//     data: &'a [T]
// }
//
// #[cfg(test)]
// mod tests {
//     use crate::gspf::{GspfHeader, HEADER_SIZE};
//
//     #[test]
//     fn i_know_how_packing_works() {
//         assert_eq!(std::mem::size_of::<GspfHeader>(), HEADER_SIZE);
//     }
// }
//
// struct GspfDePacketizer {
//
// }