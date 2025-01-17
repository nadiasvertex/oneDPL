namespace oneapi
{
namespace dpl
{
namespace __omp_backend
{

template <typename _RandomAccessIterator, typename _Compare, typename _LeafSort>
void
__parallel_stable_partial_sort(_RandomAccessIterator __xs, _RandomAccessIterator __xe, _Compare __comp,
                               _LeafSort __leaf_sort, std::size_t __nsort)
{
    //TODO: "Parallel partial sort needs to be implemented.");
    __leaf_sort(__xs, __xe, __comp);
}

} // namespace __omp_backend
} // namespace dpl
} // namespace oneapi
