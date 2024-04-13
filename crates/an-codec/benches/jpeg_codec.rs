use std::fs;
use std::fs::File;
use std::io::{Read, Write};
use std::path::Path;
use std::time::Duration;
use criterion::{black_box, criterion_group, criterion_main, Criterion};
use turbojpeg::{Compressor, Decompressor};

const WIDTH: usize = 1536;
const HEIGHT: usize = 864;

const SIZE: usize = WIDTH * HEIGHT * 3 / 2;

fn get_input(name: &str) -> Vec<u8> {
    let test_image_path = Path::new(
        env!("CARGO_MANIFEST_DIR")
    ).join("assets").join(name);
    let mut f = File::open(&test_image_path).expect("test image not found");
    let metadata = fs::metadata(&test_image_path).expect("unable to read test image metadata");
    let mut buffer = vec![0; metadata.len() as usize];
    f.read(&mut buffer).expect("buffer overflow on test image");
    buffer
}

fn write_output(out: &[u8], name: &str) {
    let output_path = Path::new(
        env!("CARGO_MANIFEST_DIR")
    ).join("assets").join(format!("{}", name).as_str());
    let mut f = File::create(&output_path).expect("Couldn't open file to write");
    f.write_all(out).expect(format!("Failed to write output file {}", name).as_str());
}

fn tj_encode(compressor: &mut Compressor, data: &[u8], res: &mut [u8]) -> Result<usize, Box<dyn std::error::Error>> {
    let input = turbojpeg::YuvImage{
        pixels: data,
        width: WIDTH,
        align: 4,
        height: HEIGHT,
        subsamp: turbojpeg::Subsamp::Sub2x2,
    };
    let output = compressor.compress_yuv_to_slice(input, res)?;
    Ok(output)
}

fn tj_decode(decompressor: &mut Decompressor, data: &[u8], res: &mut[u8]) {
    let output = turbojpeg::YuvImage{
        pixels: res,
        width: WIDTH,
        align: 4,
        height: HEIGHT,
        subsamp: turbojpeg::Subsamp::Sub2x2
    };
    decompressor.decompress_to_yuv(data, output).unwrap();
}

fn criterion_benchmark(c: &mut Criterion) {
    let data = get_input("in.yuv");
    let mut res = vec![0; SIZE];
    let mut output = 0usize;

    let mut compressor = Compressor::new().unwrap();
    compressor.set_quality(75).unwrap();

    let mut group = c.benchmark_group("JPEG Codecs");
    group.measurement_time(Duration::from_secs(45));
    group.warm_up_time(Duration::from_secs(10));

    group.bench_function("turbojpeg encode 75", |b| {
        b.iter(|| {
            output = tj_encode(
                black_box(&mut compressor),
                black_box(&data),
                black_box(&mut res),
            ).unwrap()
        })
    });

    write_output(&res[..output], "turbo_jpeg_encode.jpeg");

    let data = get_input("in.jpeg");
    let mut decompressor = Decompressor::new().unwrap();

    group.bench_function("turbojpeg decode", |b| {
        b.iter(|| {
            tj_decode(
                black_box(&mut decompressor),
                black_box(&data),
                black_box(&mut res),
            )
        })
    });

    write_output(&res, "turbo_jpeg_decode.yuv");

    group.finish();
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);