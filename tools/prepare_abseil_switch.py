"""Minimal Horizon adaptations to an Abseil copy under build/."""
import hashlib
from pathlib import Path
from local_dependencies import RAIZ, fuente


def preparar():
    raiz=fuente("abseil-cpp")
    clave=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()[:12]
    salida=RAIZ / "build" / ("abseil-switch-"+clave)
    cambios={
        "absl/time/internal/cctz/src/time_zone_libc.cc": [
            ("#elif defined(__sun) || defined(_AIX)", """#elif defined(__SWITCH__)
long tm_gmtoff(std::tm tm) {
  const auto t = std::mktime(&tm);
  std::tm utc{};
  if (!gmtime_r(&t, &utc)) return 0;
  const civil_second a(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  const civil_second b(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec);
  return static_cast<long>(a - b);
}
const char* tm_zone(const std::tm& tm) { return tzname[tm.tm_isdst > 0]; }
#elif defined(__sun) || defined(_AIX)"""),
            ("al.offset = static_cast<int>(tm_gmtoff(*tmp));", "al.offset = local_ ? static_cast<int>(tm_gmtoff(*tmp)) : 0;")],
        "absl/debugging/internal/elf_mem_image.h": [("!defined(__VXWORKS__) && !defined(__hexagon__)","!defined(__VXWORKS__) && !defined(__hexagon__) && !defined(__SWITCH__)")],
        "absl/base/internal/sysinfo.cc": [('#include "absl/base/internal/sysinfo.h"','#include "absl/base/internal/sysinfo.h"\n#ifdef __SWITCH__\n#include <switch.h>\n#endif'),
            ("#elif defined(__native_client__)","#elif defined(__SWITCH__)\npid_t GetTID() {\n  uint64_t id = 0;\n  if (R_FAILED(svcGetThreadId(&id, CUR_THREAD_HANDLE))) std::abort();\n  return static_cast<pid_t>(id);\n}\n\n#elif defined(__native_client__)")],
    }
    for nombre,reemplazos in cambios.items():
        texto=(raiz / nombre).read_text(encoding="utf-8")
        for a,b in reemplazos:
            if texto.count(a)!=1:raise ValueError("Cambio de fuente Abseil inesperado: "+nombre)
            texto=texto.replace(a,b)
        destino=salida / nombre;destino.parent.mkdir(parents=True,exist_ok=True)
        if not destino.exists() or destino.read_text(encoding="utf-8")!=texto:destino.write_text(texto,encoding="utf-8")
    return salida
