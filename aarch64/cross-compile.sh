function do_ArmRuntimeStage() {
  current_dir="$PWD"

  if true; then
    # Download cross-compilation toolchain from https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
    AARCH64_TOOLCHAIN_FOLDER="${current_dir}/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu"
    AARCH64_TAR_XZ_URL="https://developer.arm.com/-/media/Files/downloads/gnu/11.2-2022.02/binrel/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu.tar.xz?rev=33c6e30e5ac64e6dba8f0431f2c35f1b&hash=AE0C3F32FC140B87A05846EBF9494722"
    AARCH64_TAR_XZ="$AARCH64_TOOLCHAIN_FOLDER".tar.xz

    if [ ! -f "$AARCH64_TAR_XZ" ]; then
      wget -O "$AARCH64_TAR_XZ" "$AARCH64_TAR_XZ_URL"
    fi

    if [ ! -d "$AARCH64_TOOLCHAIN_FOLDER" ]; then
      AARCH64_TAR_XZ_EXPECTED_HASH="52dbac3eb71dbe0916f60a8c5ab9b7dc9b66b3ce513047baa09fae56234e53f3"
      AARCH64_TAR_XZ_ACTUAL_HASH=$(sha256sum "$AARCH64_TAR_XZ" | awk '{ print $1 }')
      if [[ "$AARCH64_TAR_XZ_EXPECTED_HASH" != "$AARCH64_TAR_XZ_ACTUAL_HASH" ]]; then
          echo "Hash check failed"
          echo "Expected $AARCH64_TAR_XZ_EXPECTED_HASH but got $AARCH64_TAR_XZ_ACTUAL_HASH"
          exit
      fi

      mkdir -p "$AARCH64_TOOLCHAIN_FOLDER"
      tar -xvf "$AARCH64_TAR_XZ" -C "$AARCH64_TOOLCHAIN_FOLDER" --strip-components=1

      # WORKAROUND. The linker refuses to pick these objects up from the original location.
      ln -s ${current_dir}/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/lib/gcc/aarch64-none-linux-gnu/11.2.1/crtbeginS.o \
         ${current_dir}/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr/lib/crtbeginS.o
      ln -s ${current_dir}/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/lib/gcc/aarch64-none-linux-gnu/11.2.1/crtendS.o \
         ${current_dir}/gcc-arm-11.2-2022.02-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr/lib/crtendS.o
    fi
  fi

  cmake -DCMAKE_TOOLCHAIN_FILE="${current_dir}/clang_to_aarch64-linux-gnu.toolchain.cmake" -G Ninja \
      .. \
      -DCMAKE_BUILD_TYPE=Release

    DESTDIR=$DEST/clang-install-aarch64 ninja -j "$CORE_COUNT" install
}


do_ArmRuntimeStage
