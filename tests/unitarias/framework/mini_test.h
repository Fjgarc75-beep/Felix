// =====================================================================
//  mini_test.h - Marco de pruebas mínimo (sin dependencias externas)
//  Proyecto: Monitor PPG experimental - Etapa 10 (pruebas)
//
//  Cada caso declara: ID, función bajo prueba, tipo (N/L/E) y descripción.
//  Al terminar escribe:
//    reports/<suite>_resultados.md   tabla legible
//    reports/<suite>_resultados.csv  una fila por caso
//    reports/<suite>_junit.xml       formato JUnit para herramientas de CI
// =====================================================================
#pragma once
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

namespace mt {

struct Case {
  const char *id;
  const char *func;
  const char *kind;     // "N" normal, "L" límite, "E" error
  const char *desc;
  void (*fn)();
};

struct Result {
  const Case *c;
  int checks = 0;
  int failed = 0;
  std::vector<std::string> msgs;
};

inline std::vector<Case> &registry() { static std::vector<Case> r; return r; }
inline Result *&current() { static Result *r = nullptr; return r; }

struct Registrar {
  Registrar(const char *id, const char *func, const char *kind, const char *desc, void (*fn)()) {
    registry().push_back(Case{id, func, kind, desc, fn});
  }
};

inline void record(bool ok, const char *file, int line, const std::string &what) {
  Result *r = current();
  r->checks++;
  if (!ok) {
    r->failed++;
    char buf[64];
    snprintf(buf, sizeof(buf), " (%s:%d)", file, line);
    r->msgs.push_back(what + buf);
  }
}

inline std::string fmtd(double v) { char b[64]; snprintf(b, sizeof(b), "%.9g", v); return b; }

inline std::string xmlEsc(const std::string &s) {
  std::string o;
  for (char ch : s) {
    if (ch == '&') o += "&amp;"; else if (ch == '<') o += "&lt;";
    else if (ch == '>') o += "&gt;"; else if (ch == '"') o += "&quot;"; else o += ch;
  }
  return o;
}

inline std::string mdEsc(const std::string &s) {
  std::string o;
  for (char ch : s) { if (ch == '|') o += "\\|"; else if (ch == '\n') o += ' '; else o += ch; }
  return o;
}

inline int runAll(const char *suite, const char *reportDir, void (*beforeEach)()) {
  std::vector<Result> results;
  int nFail = 0;
  for (const Case &c : registry()) {
    Result r; r.c = &c;
    current() = &r;
    if (beforeEach) beforeEach();
    c.fn();
    if (r.checks == 0) { r.failed++; r.msgs.push_back("el caso no ejecutó ninguna verificación"); }
    if (r.failed) nFail++;
    printf("[%s] %-10s %-28s %s  (%d verificaciones)\n", r.failed ? "FALLA" : " OK  ", c.id, c.func, c.kind, r.checks);
    for (auto &m : r.msgs) printf("        - %s\n", m.c_str());
    results.push_back(r);
  }
  int total = (int)results.size();
  printf("\n%s: %d casos, %d aprobados, %d fallidos\n", suite, total, total - nFail, nFail);

  std::string base = std::string(reportDir) + "/" + suite;
  if (FILE *f = fopen((base + "_resultados.csv").c_str(), "w")) {
    fprintf(f, "id,funcion,tipo,resultado,verificaciones,descripcion,detalle\n");
    for (auto &r : results) {
      std::string det; for (auto &m : r.msgs) det += m + "; ";
      fprintf(f, "%s,%s,%s,%s,%d,\"%s\",\"%s\"\n", r.c->id, r.c->func, r.c->kind,
              r.failed ? "FALLA" : "OK", r.checks, r.c->desc, det.c_str());
    }
    fclose(f);
  }
  if (FILE *f = fopen((base + "_junit.xml").c_str(), "w")) {
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<testsuite name=\"%s\" tests=\"%d\" failures=\"%d\">\n", suite, total, nFail);
    for (auto &r : results) {
      fprintf(f, "  <testcase classname=\"%s\" name=\"%s %s\">", r.c->func, r.c->id, xmlEsc(r.c->desc).c_str());
      if (r.failed) {
        std::string det; for (auto &m : r.msgs) det += m + "\n";
        fprintf(f, "<failure message=\"%d verificaciones fallidas\">%s</failure>", r.failed, xmlEsc(det).c_str());
      }
      fprintf(f, "</testcase>\n");
    }
    fprintf(f, "</testsuite>\n");
    fclose(f);
  }
  if (FILE *f = fopen((base + "_resultados.md").c_str(), "w")) {
    fprintf(f, "| ID | Funcion | Tipo | Resultado | Verificaciones | Caso |\n| --- | --- | --- | --- | --- | --- |\n");
    for (auto &r : results)
      fprintf(f, "| %s | `%s` | %s | %s | %d | %s |\n", r.c->id, r.c->func, r.c->kind,
              r.failed ? "FALLA" : "OK", r.checks, mdEsc(r.c->desc).c_str());
    fclose(f);
  }
  return nFail == 0 ? 0 : 1;
}

}  // namespace mt

#define MT_CAT2(a, b) a##b
#define MT_CAT(a, b) MT_CAT2(a, b)
#define TEST_CASE(ID, FUNC, KIND, DESC)                                              \
  static void MT_CAT(tc_, ID)();                                                      \
  static mt::Registrar MT_CAT(reg_, ID)(#ID, FUNC, KIND, DESC, MT_CAT(tc_, ID));      \
  static void MT_CAT(tc_, ID)()

#define CHECK(cond) mt::record((cond), __FILE__, __LINE__, std::string("CHECK(") + #cond + ")")
#define CHECK_EQ(a, b)                                                                 \
  do { auto _va = (a); auto _vb = (b);                                                 \
       mt::record(_va == _vb, __FILE__, __LINE__, std::string(#a " == " #b ": obtenido ") \
                  + mt::fmtd((double)_va) + ", esperado " + mt::fmtd((double)_vb)); } while (0)
#define CHECK_NEAR(a, b, tol)                                                          \
  do { double _va = (double)(a), _vb = (double)(b);                                     \
       mt::record(std::fabs(_va - _vb) <= (tol) && std::isfinite(_va), __FILE__, __LINE__, \
                  std::string(#a ": obtenido ") + mt::fmtd(_va) + ", esperado " + mt::fmtd(_vb) \
                  + " +- " + mt::fmtd(tol)); } while (0)
#define CHECK_STR(a, b)                                                                \
  do { std::string _va = (a), _vb = (b);                                               \
       mt::record(_va == _vb, __FILE__, __LINE__, std::string(#a ": obtenido \"") + _va  \
                  + "\", esperado \"" + _vb + "\""); } while (0)
