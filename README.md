# SMOOTH

## About this project

[Smooth](https://github.com/loilo-inc/smooth) is a After Effects plug-in, created by Loilo, that makes animation cells smooth.

Animation cells are painted with no anti-alias, so we must make the cells smooth in composition process. Usually, After Effects is used in such a composition process, so Smooth is developed as a After Effects plug-in.

But now we try DaVinci Resolve to replace After Effects. But DaVinci Resolove has no function to smooth animation cells. So we are trying to port Smooth to work in DaVinci Resolve.

## How it works
### In After Effects
![AEoriginal](img/AEoriginal.png)
This is an original image with no anti-alias.

![AErange](img/AErange.png)
*Range* sets the pixel size of smoothing.
- MIN 0
- MAX 10
- DEF 1

![AElineWeight](img/AElineWeight.png)
*LineWeight* sets the weight of the line..
- MIN 0
- MAX 1
- DEF 0
![AEtransparent](img/AEtransparent.png)
Checked *Transparent*, white pixels will be transparent.

### In DaVinci Resolve
![OFXoriginal](img/OFXoriginal.png)
In DaVinci Resolve, it is displayed as this image.

## OpenFX API

OpenFX is the plug-in standard adopted by DaVinci Resolve. To work in DaVinci Resolve, we must port the plug-in to OpenFX plug-in. [Here](https://github.com/ofxa/openfx) is OpenFX repository.

## After Effects SDK

Developed as After Effects plug-in, Smooth has many functions that depend on After Effects SDK. So we must know this SDK's specification, to port the plug-in to OpenFX. We are able to download After Effects SDK in [here](https://console.adobe.io/downloads/ae).

## Developing environment

- MacBookAir(Intel Core i5),  MacBook Air(Apple M1)
- macOS 11.1
- clang++
- GNU make
- XCode 12.3
- DaVinci Resolve 17.1 beta

### Directory tree
```
|
`- sdk/ (rename of After Effects SDK 2020)
`- openfx/
`- Smooth/
```

### Git branch
```
master
develop
mock
```
*master* is clean branch. we only added OpenFX headers to `Util.h` and `Effect.cpp`, created `Makefile` and `.gitignore`.

We tried something in *develop* branch. In this branch, we have compire error and could not to built.

In *mock* branch, we are able to built with `make` command and install with `make install` command,  DaVinci Resolve can read the plug-in with no error, correct GUI can be displayed, but it does not work.

### How to built
#### Usable After Effects plug-in
In master branch, open `Mac/smooth.xcodeproj` and built.

#### OpenFX plug-in
```sh
$ cd Smooth/
$ make
$ make install
```

# To contributors
We are looking for contributors to help us.  We are preparing rewards for those who help.

## Environment
- Fork this repository and clone to your local machine.
- Need not to match environment to ours, such as OS, compiler, and any software versions. But let us know your environment.

## Coding
- We have no coding rules.
- Use English when writing natural language, such as comment, commit message, etc.
- When disable the original code, comment out it never erasing.

## Testing
- Need not to write test codes.

## Complite
- Please pull request.
- Success to built and correct operation in our local machine, we will merge your pull request.
- That is everything to do!