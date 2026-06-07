# psp_mkiso
Um pequeno repositório com uma ferramenta para montar ISOs para PSP.

Esse repositório foi feito inicialmente para fazer uma pequena ferramenta que consiga montar a ISO para a o patch de tradução da versão de PSP do Remember11 (A versão que pode ser aplicada pelo Android), e provavelmente vai ser modificado (se preciso) para funcionar com patches futuros para PSP.

Eu literalmente só fiz isso porque não achei nenhuma outra ferramenta para montar ISOs no Android que funcionasse para mim (Encontrei o `xorriso` no termux, mas ele não tinha suporte para o que eu precisava para a ISO do Remember11, especificamente para o equivalente a flag `-xa` usada na ferramenta `mkisofs`)

Minha ferramenta acabou sendo muito menor e funciona bem pro meu uso, então estou satisfeito.

Ela atualmente tem muitos problemas, que realmente não importam muito pra minha aplicação:
- Assume coisas que impedem que ela seja totalmente genérica, como tamanhos hardcoded para memória estática, assume que alguns cabeçários vão sempre ocupar 1 setor, usa códigos de jogos de PSP hardcoded. (Coisas que só planejo mudar caso necessário nos meus usos futuros)
- Só funciona em plataformas POSIX (porque usa `openat`, `mmap`, `readdir`, `stat`, etc)
- Ainda falta um trabalho maior para deixar o código mais organizado e fácil de entender

As pessoas podem encontrar mais problemas com o código, e realmente eu ENTENDO, mas atualmente essa ferramenta funciona e eu tenho mais jogos pra traduzir do que tempo pra ficar me preocupando com isso.

Dito isso, o repositório ainda é uma forma útil de saber como o formato ISO funciona e como você poderia montar/extrair seus próprios arquivos ISO com um código direto e (possivelmente) simples em C, é por isso que fiz ele ser público.