"""
CLI natural para el sistema de memoria del agente.
Permite interactuar con la memoria usando comandos en lenguaje natural.

Uso:
    python memory_cli.py
    python memory_cli.py "recuerda que cambié el puerto a 5000"
    python memory_cli.py "qué sabes del upload?"
"""

import sys
import re
from agent_memory import AgentMemory, MemoryService


class NaturalMemoryCLI:
    """Parsea comandos en lenguaje natural y los traduce a operaciones de memoria."""

    def __init__(self):
        self.memory = MemoryService.get_instance()

    PATRONES_GUARDAR = [
        r"^(recuerda|guarda|anota|registra|memoriza)",
        r"^(haz memoria|ten en cuenta|no olvides)",
        r"^(esto es importante|para el futuro)",
    ]

    PATRONES_BUSCAR = [
        r"^(qué sabes|que sabes|qué hay|que hay|qué tengo|que tengo|qué recuerdas|que recuerdas)",
        r"^(busca|muestra|dime sobre|cuéntame sobre|cuentame sobre)",
        r"^(se sabe algo|hay algo sobre|sabes algo)",
    ]

    PATRONES_BORRAR = [
        r"^(borra|elimina|quita|olvida)",
    ]

    PATRONES_RESUMEN = [
        r"^(resumen|summary|estado|cómo vamos|como vamos|qué tenemos|que tenemos)",
    ]

    PATRONES_RECIENTE = [
        r"^(último|ultimo|reciente|lo último|lo ultimo|últimos cambios|ultimos cambios)",
        r"^(qué hicimos|que hicimos|qué se hizo|que se hizo)",
    ]

    PATRONES_AYUDA = [
        r"^(ayuda|help|cómo se usa|como se usa|opciones|comandos)",
    ]

    def _detectar_intent(self, texto: str) -> str:
        """Detecta qué quiere hacer el usuario."""
        texto_lower = texto.lower().strip()

        for patron in self.PATRONES_GUARDAR:
            if re.match(patron, texto_lower):
                return "guardar"

        for patron in self.PATRONES_BUSCAR:
            if re.match(patron, texto_lower):
                return "buscar"

        for patron in self.PATRONES_BORRAR:
            if re.match(patron, texto_lower):
                return "borrar"

        for patron in self.PATRONES_RESUMEN:
            if re.match(patron, texto_lower):
                return "resumen"

        for patron in self.PATRONES_RECIENTE:
            if re.match(patron, texto_lower):
                return "reciente"

        for patron in self.PATRONES_AYUDA:
            if re.match(patron, texto_lower):
                return "ayuda"

        return "buscar"

    def _extraer_query(self, texto: str) -> str:
        """Extrae la parte relevante del texto después del comando."""
        texto = texto.strip()
        texto = re.sub(
            r"^(recuerda que|guarda que|anota que|registra que|memoriza que)\s*",
            "", texto, flags=re.IGNORECASE
        )
        texto = re.sub(
            r"^(recuerda|guarda|anota|registra|memoriza)\s+",
            "", texto, flags=re.IGNORECASE
        )
        texto = re.sub(
            r"^(qué sabes de|que sabes de|qué hay de|que hay de|dime sobre|cuéntame sobre|cuentame sobre|busca)\s*",
            "", texto, flags=re.IGNORECASE
        )
        texto = re.sub(
            r"^(borra|elimina|quita|olvida)\s*",
            "", texto, flags=re.IGNORECASE
        )
        return texto.strip()

    def _detectar_categoria(self, texto: str) -> str:
        """Intenta detectar la categoría basada en palabras clave."""
        texto_lower = texto.lower()

        palabras = {
            "architecture": ["arquitectura", "patrón", "estructura", "diseño", "modulo", "módulo"],
            "development": ["feature", "función", "agreg", "cambie", "desarrollo", "nuevo", "versión"],
            "bugfix": ["bug", "error", "falla", "arregl", "soluc", "fix", "problema"],
            "configuration": ["config", "puerto", "variable", "parametro", "ajuste", "settings"],
            "deployment": ["deploy", "subí", "subi", "servidor", "tunnel", "producción", "produccion"],
            "api": ["endpoint", "ruta", "api", "response", "request", "get", "post"],
            "learnings": ["aprendí", "aprendi", "lección", "leccion", "mejor práctica", "tip"],
        }

        for cat, keywords in palabras.items():
            if any(kw in texto_lower for kw in keywords):
                return cat

        return "development"

    def _detectar_importancia(self, texto: str) -> float:
        """Detecta importancia por palabras clave."""
        texto_lower = texto.lower()
        if any(w in texto_lower for w in ["importante", "crítico", "critico", "urgente", "no olvides"]):
            return 0.9
        if any(w in texto_lower for w in ["menor", "detalle", "cosita"]):
            return 0.5
        return 0.7

    def _guardar(self, texto: str):
        """Guarda una memoria."""
        contenido = self._extraer_query(texto)
        if not contenido:
            contenido = texto

        categoria = self._detectar_categoria(contenido)
        importancia = self._detectar_importancia(contenido)

        entry = self.memory.add(
            content=contenido,
            category=categoria,
            importance=importancia,
        )

        print(f"Guardado: {contenido}")
        print(f"Categoría: {categoria} | Importancia: {importancia}")

    def _buscar(self, texto: str):
        """Busca memorias."""
        query = self._extraer_query(texto)
        if not query:
            query = texto

        resultados = self.memory.search(query=query, top_k=5)

        if not resultados:
            print(f"No encontré nada sobre '{query}'")
            return

        print(f"Encontré {len(resultados)} memoria(s) sobre '{query}':")
        print("-" * 50)
        for i, m in enumerate(resultados, 1):
            print(f"  {i}. [{m.category}] {m.content}")
            if m.tags:
                print(f"     Tags: {', '.join(m.tags)}")
            print(f"     Importancia: {m.importance} | Accesos: {m.access_count}")
            print()

    def _borrar(self, texto: str):
        """Borra una memoria por ID o query."""
        query = self._extraer_query(texto)
        if not query:
            print("Dime qué quieres borrar o dame el ID")
            return

        resultados = self.memory.search(query=query, top_k=1)
        if resultados:
            m = resultados[0]
            self.memory.delete(m.id)
            print(f"Borrado: {m.content}")
        else:
            print(f"No encontré nada para borrar con '{query}'")

    def _resumen(self, texto=""):
        """Muestra resumen de memorias."""
        summary = self.memory.get_summary()
        print(f"Total memorias: {summary['total_memories']}")
        print(f"Última actualización: {summary['last_updated']}")
        print()
        print("Por categoría:")
        for cat, count in summary["by_category"].items():
            if count > 0:
                print(f"  {cat}: {count}")

    def _reciente(self, texto=""):
        """Muestra memorias recientes."""
        recientes = self.memory.get_recent(limit=5)
        if not recientes:
            print("No hay memorias aún")
            return

        print("Lo más reciente:")
        print("-" * 50)
        for i, m in enumerate(recientes, 1):
            print(f"  {i}. [{m.category}] {m.content}")
            print(f"     {m.timestamp[:16]}")
            print()

    def _ayuda(self, texto=""):
        """Muestra ayuda."""
        print("=" * 50)
        print("COMANDOS DE MEMORIA")
        print("=" * 50)
        print()
        print("Guardar:")
        print('  "recuerda que cambié el puerto a 5000"')
        print('  "guarda que se agregó soporte para múltiples sensores"')
        print('  "anota: el bug se arregla reiniciando el servicio"')
        print()
        print("Buscar:")
        print('  "qué sabes del upload?"')
        print('  "dime sobre la configuración"')
        print('  "busca sensor"')
        print()
        print("Borrar:")
        print('  "borra lo del puerto"')
        print('  "olvida el bug del csv"')
        print()
        print("Otros:")
        print('  "resumen"       - Ve todas las categorías')
        print('  "último"        - Ve lo más reciente')
        print('  "qué hicimos"   - Ve desarrollos recientes')
        print('  "ayuda"         - Muestra esta ayuda')
        print()
        print("=" * 50)

    def ejecutar(self, texto: str):
        """Ejecuta un comando natural."""
        intent = self._detectar_intent(texto)

        acciones = {
            "guardar": self._guardar,
            "buscar": self._buscar,
            "borrar": self._borrar,
            "resumen": self._resumen,
            "reciente": self._reciente,
            "ayuda": self._ayuda,
        }

        accion = acciones.get(intent, self._buscar)
        accion(texto)


def main():
    cli = NaturalMemoryCLI()

    if len(sys.argv) > 1:
        comando = " ".join(sys.argv[1:])
        cli.ejecutar(comando)
    else:
        print("Memoria del Agente AWTAS")
        print('Escribe un comando o "ayuda" para empezar.')
        print('Ej: "recuerda que cambié X"')
        print("Ctrl+C para salir.")
        print("-" * 50)

        while True:
            try:
                entrada = input("\n> ").strip()
                if not entrada:
                    continue
                if entrada.lower() in ("salir", "exit", "q"):
                    break
                cli.ejecutar(entrada)
            except KeyboardInterrupt:
                print("\nSaliendo...")
                break
            except Exception as e:
                print(f"Error: {e}")


if __name__ == "__main__":
    main()
