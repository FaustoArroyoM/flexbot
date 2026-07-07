# Third-Party Notices

This project includes third-party code that is **not** covered by the project's
`LICENSE` (MIT) and is **not** the copyright of the FlexBot authors. Each item
below retains the license and attribution of its original author.

---

## serialib

- **Files:** `serialcomm_c/include/serialib.h`, `serialcomm_c/src/serialib.cpp`
- **Author:** Philippe Lucidarme — University of Angers
- **Upstream:** <https://github.com/imabot2/serialib>
- **Version vendored:** 2.0 (27 December 2019), vendored unmodified as-is —
  no local patches.
- **Role in this project:** Cross-platform serial-port communication used by the
  PC-side host (`serialcomm.cpp`). Vendored unmodified.

**Original notice (reproduced verbatim from the source header):**

> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE X
> CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
> ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
> WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
>
> This is a licence-free software, it can be used by anyone who try to build a
> better world.

**License note:** the original header does not name an OSI-approved license.
It pairs an MIT/X11-style warranty disclaimer with a "licence-free ... can be
used by anyone" dedication — the author's intent reads as free, attribution-
preserving use, not a formal license grant. This project treats it under that
plain-language intent rather than silently relabeling it as MIT.

The `serialib` files are treated as read-only in this repository and must not be
modified (see `CLAUDE.md`). All original attribution is preserved in the file
headers.
