use std::fs;
use std::fs::File;
use std::path::Path;
use std::io::{Read, Write};
use turbojpeg::{Compressor, Decompressor};
use turbojpeg::libc::write;

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

fn turbo_jpeg_encode(data: &[u8], res: &mut [u8]) -> Result<usize, Box<dyn std::error::Error>> {
    let mut compressor = Compressor::new()?;
    compressor.set_quality(75)?;
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

fn turbo_jpeg_decode(data: &[u8], res: &mut [u8]) -> Result<(), Box<dyn std::error::Error>> {
    let mut decompressor = Decompressor::new()?;
    let output = turbojpeg::YuvImage {
        pixels: res,
        width: WIDTH,
        align: 4,
        height: HEIGHT,
        subsamp: turbojpeg::Subsamp::Sub2x2
    };
    decompressor.decompress_to_yuv(data, output)?;
    Ok(())
}


fn main() -> Result<(), Box<dyn std::error::Error>> {
    // ENCODE
    let data = get_input("in.yuv");
    let mut res = vec![0; SIZE];
    let output = turbo_jpeg_encode(&data, &mut res)?;
    write_output(&res[..output], "turbo_jpeg_encode.jpeg");

    // DECODE
    let data = get_input("in.jpeg");
    turbo_jpeg_decode(&data, &mut res)?;
    write_output(&res, "turbo_jpeg_decode.yuv");

    Ok(())
}