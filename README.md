# psp_mkiso
A small repository with a tool for building ISO files for PSP.

This repository was initially made as a way to have a small tool for building the ISO for the Remember11 Portuguese Brazilian PSP patch (Android version), and will probably be changed (if needed) for working with future PSP patches too.

I made this because I didn't find any ready to use tool for building ISOs on Android (I found `xorriso` on termux, but it lacked the support needed for Remember11 ISO to work, namely a equivalent to the `-xa` flag used on `mkisofs`).

My tool is really smaller and works for my use case so it's all good.

It currently has many problems, which may not matter to my application : 
- It makes assumptions that may not be as general purpose like hardcoded sizes on static memory, assumes some headers will only use a single sector, uses a hardcoded PSP game code. (Which I only plan on changing only if needed on my future use cases)
- It only works on POSIX platforms (because it uses openat, mmap, readdir, stat, etc)
- Still lacks some work to make the code more readable and easy to understand

People may find more problems with this, and really I HEAR YOU, but currently this tool works and I have more games to translate to really care about that.

With all that said, the repository is still a usefull way to know how the ISO format works and how you could make/extract your own ISO files with a straightfoward and (possibly) simple C code, that's why I made it public.