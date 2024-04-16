use std::time::Duration;
use criterion::{black_box, criterion_group, criterion_main, Criterion};
use an_net_codec::{GspfDepacketizer, GspfHeader, GspfImageFormat, GspfPacketizer};

const SIZE: usize = 1990656;

fn gspf_packetize(packetizer: &mut GspfPacketizer, data: &[u8]) {
    for packet in packetizer.new_packetizer(data) {
        black_box(packet);
    }
}

fn gspf_depacketize(
    depacketizer: &mut GspfDepacketizer, headers: &mut [GspfHeader], frame: &mut u16
) {
    let mut iter = headers.iter_mut();
    while let Some(header) = iter.next() {
        header.frame = *frame;
        let resp = depacketizer.receive_packet_header(header);
        black_box(resp);
    }
    *frame = frame.wrapping_add(1);
}

fn criterion_benchmark(c: &mut Criterion) {

    let data = vec![7u8; SIZE];

    let mut group = c.benchmark_group("GSPF");
    group.measurement_time(Duration::from_secs(45));
    group.warm_up_time(Duration::from_secs(10));

    let mut packetizer = GspfPacketizer::new(
        GspfImageFormat::STRING, (1, 1), None
    );

    group.bench_function("GSPF Packetize", |b| {
        b.iter(|| {
            gspf_packetize(
                black_box(&mut packetizer),
                black_box(&data)
            );
        })
    });

    let mut packet_headers = packetizer.new_packetizer(&data).map(
        |send_packet| -> GspfHeader { send_packet.header }
    ).collect::<Vec<GspfHeader>>();

    let mut frame = 0u16;

    let mut depacketizer = GspfDepacketizer::new(
        GspfImageFormat::STRING, (1, 1), SIZE, None
    );

    group.bench_function("GSPF Depacketize", |b| {
        b.iter(|| {
            gspf_depacketize(
                black_box(&mut depacketizer),
                black_box(&mut packet_headers),
                black_box(&mut frame)
            )
        })
    });

    group.finish();
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);