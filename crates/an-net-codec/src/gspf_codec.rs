use std::io::Write;
use bincode::Options;
use tokio_util::bytes::{BufMut, BytesMut};
use tokio_util::codec::{Decoder, Encoder};
use crate::gspf::{GspfHeader, GspfReceivePacket, GspfSendPacket, MTU, HEADER_SIZE};

const HOLE_PUNCH: u8 = 0b11000000;

pub enum GspfSend<'a> {
    Packet(GspfSendPacket<'a>),
    HolePunch
}

pub enum GspfReceive {
    Packet(GspfReceivePacket),
    HolePunch
}


pub struct GspfSender {
    options: bincode::DefaultOptions,
    mtu: usize
}

impl<'a> GspfSender {
    pub fn new(mtu: Option<usize>) -> Self {
        let mtu = mtu.unwrap_or(MTU);
        assert!(mtu > HEADER_SIZE);
        Self {
            mtu,
            options: Default::default()
        }
    }

    fn encode_send_packet(
        &mut self, GspfSendPacket { header, data }: GspfSendPacket<'a>, dst: &mut BytesMut
    ) -> Result<(), std::io::Error> {
        let size = header.size as usize;
        if size != data.len() {
            // todo: data incorrect
        } else if size + HEADER_SIZE > self.mtu {
            // todo: packet too big
        }
        let mut writer = dst.writer();
        // todo: handle serialization error
        self.options.clone().serialize_into(&mut writer, &header).unwrap();
        // todo: handle memcpy error
        writer.write(data).unwrap();
        Ok(())
    }
}

impl<'a> Encoder<GspfSend<'a>> for GspfSender {
    // todo: more specific error type
    type Error = std::io::Error;

    fn encode(&mut self, packet: GspfSend<'a>, dst: &mut BytesMut) -> Result<(), Self::Error> {
        use GspfSend::{Packet, HolePunch};

        dst.reserve(self.mtu);
        return match packet {
            Packet(packet) => {
                self.encode_send_packet(packet, dst)
            }
            HolePunch => {
                dst.put_u8(HOLE_PUNCH);
                Ok(())
            }
        }
    }
}

impl Decoder for GspfSender {
    type Item = GspfReceive;
    // todo: more specific error type
    type Error = std::io::Error;

    fn decode(&mut self, src: &mut BytesMut) -> Result<Option<Self::Item>, Self::Error> {

        use GspfReceive::{HolePunch};

        if src.len() == 1 {
            // todo: not sure if we should try and READ the byte
            return Ok(Some(HolePunch));
        }

        // todo: should throw error, should only hole punch
        Ok(None)
    }
}


pub struct GspfReciever {
    options: bincode::DefaultOptions,
    mtu: usize
}

impl GspfReciever {
    pub fn new(mtu: Option<usize>) -> Self {
        let mtu = mtu.unwrap_or(MTU);
        assert!(mtu > HEADER_SIZE);
        Self {
            mtu,
            options: Default::default()
        }
    }
}

impl Decoder for GspfReciever {
    type Item = GspfReceive;
    // todo: more specific error type
    type Error = std::io::Error;

    fn decode(&mut self, src: &mut BytesMut) -> Result<Option<Self::Item>, Self::Error> {

        use GspfReceive::{Packet, HolePunch};

        src.reserve(self.mtu);

        if src.len() == 1 {
            return Ok(Some(HolePunch));
        }

        if src.len() > self.mtu {
            // todo: packet too big
        } else if src.len() <= HEADER_SIZE {
            // todo: packet too small
        }
        // todo: bad serialization
        let header: GspfHeader = self.options
            .clone()
            .deserialize_from(src.split_to(HEADER_SIZE).as_ref())
            .unwrap();

        if header.size as usize != src.len() {
            // todo: header lying about data size
        }

        Ok(Some(Packet(GspfReceivePacket {
            header,
            data: src.split_to(src.len())
        })))
    }
}

impl<'a> Encoder<GspfSend<'a>> for GspfReciever {
    // todo: more specific error type
    type Error = std::io::Error;
    fn encode(
        &mut self, packet: GspfSend<'a>, dst: &mut BytesMut
    ) -> Result<(), Self::Error> {
        match packet {
            GspfSend::HolePunch => {
                dst.put_u8(HOLE_PUNCH);
                Ok(())
            }
            GspfSend::Packet(_) => {
                // todo: throw error, shouldn't be sending packets
                Ok(())
            }
        }
    }
}