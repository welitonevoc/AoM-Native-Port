O repositório https://github.com/ptasev/Age-of-Mythology é um projeto de libraria e ferramentas em C# que lida com arquivos de AoM (BAR, XMB, BRG/GRN, etc.).  
Ele não é escrito em C++/Android, então não pode ser copiado livremente como fonte‑nativa, mas contém:

Parte do repo	O que faz	Como pode ajudar no seu port
src/AoMEngineLibrary/Data/Bar	Leitura, mapeamento (mmap) e extração de arquivos BAR; suporte a múltiplos mounts/overlay.	Conceitos de “mount + overlay” e API pública podem ser re‑implementados em C++; serve como referência para lidar com a expansão “The Titans”.
src/AoMEngineLibrary/Data/XmbFile	Parser XMB de unidades, tecnologias, deuses, etc.	Podemos traduzir essa lógica para C++ (já temos um parser XMB relativamente completo no seu projeto) para garantir consistência entre dados.
src/**/Graphics/**	Conversores de recursos (PNG, DDS, BRG‑GRN, cubemaps, etc.).	Úteis se precisar converter assets do formato do Unity/Steam para o que o seu engine usa.
tests/AoMEngineLibrary.Tests/**	Testes unitários C# (XUnit).	Referência de cobertura e casos de teste que poderia replicar em testes C++/GoogleTest.
src/AoMEngineLibrary/	Extensões úteis (BinaryReader… ).	O mesmo conceito pode ser portado para C++ helpers.

Como podemos aproveitar?
1. Overlay de BAR – A lógica do repo para BarArchive com múltiplos mounts pode ser transcrita para o seu código existente. Se você já tem um sistema de BarArchive em C++, basta governar o “último mount” como prioridade. Isso elimina a necessidade de reescrever a sandbox de expansão.
2. Parser XMB – O código C# tem uma implementação robusta de XMB sem dependências de terceiros (explora tags, strings, etc.). Podemos usar o layout e testes do repo como base para escrever ou validar o seu parser C++ (caso ainda não seja 100% completo).
3. Conversores de Assets – Se precisar gerar PNGs, DDS ou outros formatos suportados, o repo já tem ferramentas de conversão que podem ser usadas como apontamento (código C# não pode ser usado diretamente, mas a lógica está lá).
4. Testes – Crie testes C++ que replicam os testes XUnit do repo. Isso aumenta a cobertura e a confiança de novas mudanças laterais.
Próximos passos práticos

Tarefa	Por que	Como fazer
Analise o BarArchive	Confirmar que a lógica de overlay atual (no seu BarArchive) cobre todas as regras do repo, principalmente a prioridade do último mount.	Abra src/AoMEngineLibrary/Data/Bar/BarArchive.cs e compare com include/assets/bar_archive.h/src/assets/bar_archive.cpp.
Reimplante a lógica de XMB	Logo que a sua engine use o mesmo parser, podemos reutilizar referências de estrutura de campos e testes.	Mapear classes C# (ex. XmbFile) para C++ classes equivalentes; usar os testes do repo como especificação.
Comparar testes de overlay	Validar que suas novas versões de test_bar_overlay executem o mesmo conjunto de verificações.	Executar tests/AoMEngineLibrary.Tests/Data/Bar/BarFileTests.cs no repo (via .NET) e reproduzir esses casos em C++.
Avaliar a parte de conversores	Se quiser atualizar assets via um conversor de linha de comando, as classes C# podem servir de blueprint.	Deixar o projeto C# como “referência” e criar ferramenta CLI em C++ (ou manter um wrapper em C# apenas para conversão).

Conclusão
Você não pode simplesmente copiar o código para o seu projeto de Android, pois é C#/.NET, mas pode usar os algoritmos e estrutura das classes mencionadas como referência para re‑implementar em C++.  

O repositório oferece ótimas ideias de design (overlay, parsing, conversores) que encaixam bem no seu port se você escrever a camada C++ correspondente.
Se quiser que eu ajude a escrever ou transformar algum módulo específico (por ex. a classe BarArchive ou o parser XMB) em C++, por favor indique qual arquivo ou funcionalidade precisa priorizar.