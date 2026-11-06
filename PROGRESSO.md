# Progresso — AoM Native Port

## Última atualização

### Adição dos testes de referência (`tests/reference/`)
- Incluídas as pastas: `databar`, `databar2`, `ddt`, `sounds`, `sounds2`, `textures`, `textures2`
- Dados de referência extraídos dos BARs do Age of Mythology, usados para estudo e comparação de conversão
- Envio feito com `git add -f` pois a pasta está ignorada no `.gitignore` (dados proprietários do jogo original)
- Commit: `adiciona testes/reference para fins de estudo`
- Branch: `main` — sincronizado com `origin/main` no GitHub

## Observações
- O repositório está 6 commits à frente do que já estava no GitHub antes deste envio
- `CMakeLists.txt`: removida referência inexistente a `third_party/stb_image.c` do alvo `test_ddt_reference` (commit `fix: remover STB_IMAGE_WRITE inexistente do test_ddt_reference`)
- Nenhuma alteração pendente no working tree
