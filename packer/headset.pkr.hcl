packer {
  required_plugins {
    arm-image = {
      version = ">= 0.2.5"
      source  = "github.com/solo-io/arm-image"
    }
  }
}

source "arm-image" "raspios_lite_arm64" {
  image_type = "raspberrypi"
  iso_url = "https://downloads.raspberrypi.org/raspios_lite_arm64/images/raspios_lite_arm64-2023-05-03/2023-05-03-raspios-bullseye-arm64-lite.img.xz"
  iso_checksum = "8d438cace321b4e2fa32334d620d821197e188fe"
  output_filename = "images/headset.img"
  qemu_binary     = "qemu-aarch64-static"
  mount_path    = "/mnt/headset_raspios_lite_arm64"
  target_image_size =  3969908736
}

build {
  sources = ["source.arm-image.raspios_lite_arm64"]
  provisioner "ansible" {
    playbook_file = "headset.ans.yml"
    ansible_env_vars = [
      "ANSIBLE_FORCE_COLOR=1",
      "PYTHONUNBUFFERED=1",
    ]
    extra_arguments  = [
      # The following arguments are required for running Ansible within a chroot
      # See https://www.packer.io/plugins/provisioners/ansible/ansible#chroot-communicator for details
      "--connection=chroot",
      "--become-user=root",
      #  Ansible needs this to find the mount path
      "-e ansible_host=${build.MountPath}"
    ]
  }
}