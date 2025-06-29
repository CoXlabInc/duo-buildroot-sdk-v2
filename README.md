# Milk-V Duo series buildroot SDK V2

For more detailed documentation, please refer to: [https://milkv.io/docs/duo/getting-started/buildroot-sdk](https://milkv.io/docs/duo/getting-started/buildroot-sdk)

# How to build

## One-click full build
Select  `3. milkv-duo256m-musl-riscv64-sd.`
```
cd duo-buildroot-sdk/
./build.sh lunch
```
The built image is located at `out/milkv-duo256m-musl-riscv64-sd-*.img`

## Step by step full build
Select `3. milkv-duo256m-musl-riscv64-sd` (once at first build)
```
cd duo-buildroot-sdk/
source build/envsetup_milkv.sh
clean_all
build_all
pack_sd_image
```
The built image is located at `install/soc_sg2002_milkv_duo256m_musl_riscv64_sd/milkv-duo256m-musl-riscv64-sd.img`



## Add packages after full build

Select `3. milkv-duo256m-musl-riscv64-sd`
```
cd duo-buildroot-sdk/
source build/envsetup_milkv.sh
```

Select packages what you want on menuconfig. Then save and exit from menuconfig.
```
cd buildroot/output/milkv-duo256m-musl-riscv64-sd/
make menuconfig
```


This will copy .config to `buildroot/configs/milkv-duo256m-musl-riscv64-sd_defconfig`
```
make savedefconfig 
```


Move back to `duo-buildroot-sdk/` and do build. This is faster than full build.
```
pack_rootfs
pack_sd_image
```
The built image is located at `install/soc_sg2002_milkv_duo256m_musl_riscv64_sd/milkv-duo256m-musl-riscv64-sd.img`

## ⚠️ **Warning:** 
If the package is built once before, It won't be rebuilt even if you modify the config.  
You must remove .stamp_* files on build directory of the added package.  
For example,
```
cd buildroot/output/milkv-duo256m-musl-riscv64-sd/build/ffmpeg-6.1.2
rm .stamp_*
```
And move back to `duo-buildroot-sdk-v2/`. Then rebuild.
```
cd ../../../..
pack_rootfs
pack_sd_iamge
```
The built image is located at install/soc_sg2002_milkv_duo256m_musl_riscv64_sd/milkv-duo256m-musl-riscv64-sd.img
