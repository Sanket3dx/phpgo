# Publishing Guide for `phpgo`

Since `phpgo` is a native extension (C+Go), publishing it is a bit different from a normal PHP library. Here are the three main ways to get it into other people's hands.

## 1. The Composer Way (Discovery & IDE)
You've already added `composer.json`. Now:
1. Go to [Packagist.org](https://packagist.org/).
2. Submit your GitHub URL: `https://github.com/Sanket3dx/phpgo.git`.
3. **What this does**: It allows users to run `composer require sanket3dx/phpgo`. 
4. **Note**: This *only* installs the IDE helper stubs. It **cannot** install the native `.so` file automatically because users need Go and GCC to build it.

## 2. The Native Way (PECL)
PECL is the official repository for PHP extensions. 
1. I have created a `package.xml` in your repo.
2. You need to create an account at [pecl.php.net](https://pecl.php.net/).
3. Follow the submission process to get listed.
4. **Result**: Users can install it via `pecl install phpgo`.

## 3. The Modern Way (Distribution via Binaries)
Since building a C+Go extension is hard for some users, you should provide pre-compiled binaries.
1. Use **GitHub Actions** to automatically build `phpgo.so` for Linux (Ubuntu, Debian, etc.).
2. Attach the `phpgo.so` and `libphpgo.so` to your **GitHub Release**.
3. **Instruction for Users**: 
   - Download the `.so` files.
   - Place them in their extension directory.
   - Add `extension=phpgo.so` to `php.ini`.

## 4. The Docker Way (Recommended for Production)
Provide a `Dockerfile` that users can use as a base or copy the build logic from.
```dockerfile
# Example for users
FROM php:8.2-cli
RUN apt-get update && apt-get install -y golang gcc make
# Clone and build phpgo
# ...
```

### Recommendation
For now, **Submit to Packagist**. It's the easiest first step to make your package searchable in the PHP ecosystem.
