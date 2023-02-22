#include <bcos-cpp-sdk/utilities/abi/ContractABICodec.h>
#include <bcos-crypto/hash/Keccak256.h>
#include <bcos-framework/storage2/MemoryStorage.h>
#include <bcos-framework/transaction-executor/TransactionExecutor.h>
#include <bcos-tars-protocol/protocol/BlockFactoryImpl.h>
#include <bcos-tars-protocol/protocol/BlockHeaderFactoryImpl.h>
#include <bcos-tars-protocol/protocol/TransactionFactoryImpl.h>
#include <bcos-tars-protocol/protocol/TransactionReceiptFactoryImpl.h>
#include <bcos-tars-protocol/protocol/TransactionReceiptImpl.h>
#include <bcos-task/Wait.h>
#include <bcos-transaction-executor/TransactionExecutorImpl.h>
#include <bcos-transaction-scheduler/MultiLayerStorage.h>
#include <bcos-transaction-scheduler/SchedulerParallelImpl.h>
#include <bcos-transaction-scheduler/SchedulerSerialImpl.h>
#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <transaction-executor/tests/TestBytecode.h>
#include <boost/throw_exception.hpp>
#include <range/v3/view/any_view.hpp>
#include <range/v3/view/chunk.hpp>
#include <range/v3/view/single.hpp>
#include <variant>

using namespace bcos;
using namespace bcos::storage2::memory_storage;
using namespace bcos::transaction_scheduler;
using namespace bcos::transaction_executor;

constexpr static s256 singleIssue(1000000);
constexpr static s256 singleTransfer(1);

struct TableNameHash
{
    size_t operator()(const bcos::transaction_executor::StateKey& key) const
    {
        return std::hash<bcos::transaction_executor::StateKey>{}(key);
    }
};

using MutableStorage = MemoryStorage<StateKey, StateValue, Attribute(ORDERED | LOGICAL_DELETION)>;
using BackendStorage =
    MemoryStorage<StateKey, StateValue, Attribute(ORDERED | CONCURRENT), TableNameHash>;
using MultiLayerStorageType = MultiLayerStorage<MutableStorage, void, BackendStorage>;
using ReceiptFactory = bcostars::protocol::TransactionReceiptFactoryImpl;

bcos::crypto::Hash::Ptr bcos::transaction_executor::GlobalHashImpl::g_hashImpl;

template <bool parallel>
struct Fixture
{
    Fixture()
      : m_cryptoSuite(std::make_shared<bcos::crypto::CryptoSuite>(
            std::make_shared<bcos::crypto::Keccak256>(), nullptr, nullptr)),
        m_blockHeaderFactory(
            std::make_shared<bcostars::protocol::BlockHeaderFactoryImpl>(m_cryptoSuite)),
        m_transactionFactory(
            std::make_shared<bcostars::protocol::TransactionFactoryImpl>(m_cryptoSuite)),
        m_receiptFactory(
            std::make_shared<bcostars::protocol::TransactionReceiptFactoryImpl>(m_cryptoSuite)),
        m_blockFactory(std::make_shared<bcostars::protocol::BlockFactoryImpl>(
            m_cryptoSuite, m_blockHeaderFactory, m_transactionFactory, m_receiptFactory)),
        m_multiLayerStorage(m_backendStorage),
        m_scheduler(std::conditional_t<parallel,
            SchedulerParallelImpl<MultiLayerStorageType, ReceiptFactory, TransactionExecutorImpl>,
            SchedulerSerialImpl<MultiLayerStorageType, ReceiptFactory, TransactionExecutorImpl>>(
            m_multiLayerStorage, *m_receiptFactory, m_tableNamePool))
    {
        bcos::transaction_executor::GlobalHashImpl::g_hashImpl =
            std::make_shared<bcos::crypto::Keccak256>();
        boost::algorithm::unhex(helloworldBytecode, std::back_inserter(m_helloworldBytecodeBinary));
    }

    void deployContract()
    {
        std::visit(
            [this](auto& scheduler) {
                task::syncWait([this, &scheduler]() -> task::Task<void> {
                    bcostars::protocol::TransactionImpl createTransaction(
                        [inner = bcostars::Transaction()]() mutable {
                            return std::addressof(inner);
                        });
                    createTransaction.mutableInner().data.input.assign(
                        m_helloworldBytecodeBinary.begin(), m_helloworldBytecodeBinary.end());

                    auto block = m_blockFactory->createBlock();
                    auto blockHeader = block->blockHeader();
                    blockHeader->setNumber(1);
                    blockHeader->calculateHash(*m_cryptoSuite->hashImpl());

                    auto transactions =
                        RANGES::single_view(std::addressof(createTransaction)) |
                        RANGES::views::transform([](auto* ptr) -> auto const& { return *ptr; });
                    scheduler.start();
                    auto receipts =
                        co_await scheduler.execute(*block->blockHeaderConst(), transactions);
                    if (receipts[0]->status() != 0)
                    {
                        fmt::print("deployContract unexpected receipt status: {}, {}\n",
                            receipts[0]->status(), receipts[0]->message());
                        co_return;
                    }

                    co_await scheduler.finish(*blockHeader, *(m_cryptoSuite->hashImpl()));
                    co_await scheduler.commit();

                    m_contractAddress = receipts[0]->contractAddress();
                }());
            },
            m_scheduler);
    }

    void prepareAddresses(size_t count)
    {
        std::mt19937_64 rng(std::random_device{}());

        // Generation accounts
        m_addresses = RANGES::iota_view<size_t, size_t>(0, count) |
                      RANGES::views::transform([&rng](size_t index) {
                          bcos::h160 address;
                          address.generateRandomFixedBytesByEngine(rng);
                          return address;
                      }) |
                      RANGES::to<decltype(m_addresses)>();
    }

    void prepareIssue(size_t count)
    {
        bcos::codec::abi::ContractABICodec abiCodec(
            bcos::transaction_executor::GlobalHashImpl::g_hashImpl);
        m_transactions =
            m_addresses | RANGES::views::transform([this, &abiCodec](const Address& address) {
                auto transaction = std::make_unique<bcostars::protocol::TransactionImpl>(
                    [inner = bcostars::Transaction()]() mutable { return std::addressof(inner); });
                auto& inner = transaction->mutableInner();

                inner.data.to = m_contractAddress;
                auto input = abiCodec.abiIn("issue(address,int256)", address, singleIssue);
                inner.data.input.assign(input.begin(), input.end());
                return transaction;
            }) |
            RANGES::to<decltype(m_transactions)>();
    }

    void prepareTransfer(size_t count)
    {
        bcos::codec::abi::ContractABICodec abiCodec(
            bcos::transaction_executor::GlobalHashImpl::g_hashImpl);
        m_transactions =
            m_addresses | RANGES::views::chunk(2) |
            RANGES::views::transform([this, &abiCodec](auto&& range) {
                auto transaction = std::make_unique<bcostars::protocol::TransactionImpl>(
                    [inner = bcostars::Transaction()]() mutable { return std::addressof(inner); });
                auto& inner = transaction->mutableInner();
                inner.data.to = m_contractAddress;
                auto& fromAddress = range[0];
                auto& toAddress = range[1];

                auto input = abiCodec.abiIn(
                    "transfer(address,address,int256)", fromAddress, toAddress, singleTransfer);
                inner.data.input.assign(input.begin(), input.end());
                return transaction;
            }) |
            RANGES::to<decltype(m_transactions)>();
    }

    task::Task<std::vector<s256>> balances()
    {
        co_return co_await std::visit(
            [this](auto& scheduler) -> task::Task<std::vector<s256>> {
                bcos::codec::abi::ContractABICodec abiCodec(
                    bcos::transaction_executor::GlobalHashImpl::g_hashImpl);
                // Verify the data
                bcostars::protocol::BlockHeaderImpl blockHeader(
                    [inner = bcostars::BlockHeader()]() mutable { return std::addressof(inner); });
                blockHeader.setNumber(0);

                auto checkTransactions =
                    m_addresses | RANGES::views::transform([&](const auto& address) {
                        auto transaction = std::make_unique<bcostars::protocol::TransactionImpl>(
                            [inner = bcostars::Transaction()]() mutable {
                                return std::addressof(inner);
                            });
                        auto& inner = transaction->mutableInner();
                        inner.data.to = m_contractAddress;

                        auto input = abiCodec.abiIn("balance(address)", address);
                        inner.data.input.assign(input.begin(), input.end());
                        return transaction;
                    }) |
                    RANGES::to<std::vector<std::unique_ptr<bcostars::protocol::TransactionImpl>>>();

                scheduler.start();
                auto receipts = co_await scheduler.execute(blockHeader,
                    checkTransactions |
                        RANGES::views::transform([
                        ](const std::unique_ptr<bcostars::protocol::TransactionImpl>& transaction)
                                                     -> auto& { return *transaction; }));
                co_await scheduler.finish(blockHeader, *(m_cryptoSuite->hashImpl()));

                auto balances = receipts |
                                RANGES::views::transform([&abiCodec](auto const& receipt) {
                                    if (receipt->status() != 0)
                                    {
                                        BOOST_THROW_EXCEPTION(std::runtime_error(
                                            fmt::format("Unexpected receipt status: {}, {}\n",
                                                receipt->status(), receipt->message())));
                                    }

                                    s256 balance;
                                    abiCodec.abiOut(receipt->output(), balance);
                                    return balance;
                                }) |
                                RANGES::to<std::vector<s256>>();

                co_return balances;
            },
            m_scheduler);
    }

    bcos::crypto::CryptoSuite::Ptr m_cryptoSuite;
    std::shared_ptr<bcostars::protocol::BlockHeaderFactoryImpl> m_blockHeaderFactory;
    std::shared_ptr<bcostars::protocol::TransactionFactoryImpl> m_transactionFactory;
    std::shared_ptr<bcostars::protocol::TransactionReceiptFactoryImpl> m_receiptFactory;
    std::shared_ptr<bcostars::protocol::BlockFactoryImpl> m_blockFactory;

    BackendStorage m_backendStorage;
    MultiLayerStorageType m_multiLayerStorage;

    TableNamePool m_tableNamePool;
    bcos::bytes m_helloworldBytecodeBinary;

    std::variant<
        SchedulerSerialImpl<MultiLayerStorageType, ReceiptFactory, TransactionExecutorImpl>,
        SchedulerParallelImpl<MultiLayerStorageType, ReceiptFactory, TransactionExecutorImpl>>
        m_scheduler;

    std::string m_contractAddress;
    std::vector<Address> m_addresses;
    std::vector<std::unique_ptr<bcostars::protocol::TransactionImpl>> m_transactions;
};

template <bool parallel = false>
static void issue(benchmark::State& state)
{
    Fixture<parallel> fixture;
    fixture.deployContract();

    auto count = state.range(0);
    fixture.prepareAddresses(count);
    fixture.prepareIssue(count);

    int i = 0;
    std::visit(
        [&](auto& scheduler) {
            task::syncWait([&](benchmark::State& state) -> task::Task<void> {
                for (auto const& it : state)
                {
                    bcostars::protocol::BlockHeaderImpl blockHeader(
                        [inner = bcostars::BlockHeader()]() mutable {
                            return std::addressof(inner);
                        });
                    blockHeader.setNumber((i++) + 1);

                    scheduler.start();
                    [[maybe_unused]] auto receipts = co_await scheduler.execute(blockHeader,
                        fixture.m_transactions | RANGES::views::transform([
                        ](const std::unique_ptr<bcostars::protocol::TransactionImpl>& transaction)
                                                                              -> auto& {
                            return *transaction;
                        }));

                    co_await scheduler.finish(blockHeader, *(fixture.m_cryptoSuite->hashImpl()));
                    co_await scheduler.commit();
                }

                auto balances = co_await fixture.balances();
                for (auto& balance : balances)
                {
                    if (balance != singleIssue * i)
                    {
                        BOOST_THROW_EXCEPTION(
                            std::runtime_error(fmt::format("Balance not equal to expected! {}",
                                balance.template convert_to<std::string>())));
                    }
                }
            }(state));
        },
        fixture.m_scheduler);
}

template <bool parallel>
static void transfer(benchmark::State& state)
{
    Fixture<parallel> fixture;
    fixture.deployContract();

    auto count = state.range(0) * 2;
    fixture.prepareAddresses(count);
    fixture.prepareIssue(count);

    std::visit(
        [&](auto& scheduler) {
            int i = 0;
            task::syncWait([&](benchmark::State& state) -> task::Task<void> {
                // First issue
                bcostars::protocol::BlockHeaderImpl blockHeader(
                    [inner = bcostars::BlockHeader()]() mutable { return std::addressof(inner); });
                blockHeader.setNumber(0);

                scheduler.start();
                [[maybe_unused]] auto receipts = co_await scheduler.execute(blockHeader,
                    fixture.m_transactions |
                        RANGES::views::transform([
                        ](const std::unique_ptr<bcostars::protocol::TransactionImpl>& transaction)
                                                     -> auto& { return *transaction; }));
                co_await scheduler.finish(blockHeader, *(fixture.m_cryptoSuite->hashImpl()));
                co_await scheduler.commit();

                fixture.m_transactions.clear();
                fixture.prepareTransfer(count);

                // Start transfer
                for (auto const& it : state)
                {
                    bcostars::protocol::BlockHeaderImpl blockHeader(
                        [inner = bcostars::BlockHeader()]() mutable {
                            return std::addressof(inner);
                        });
                    blockHeader.setNumber((i++) + 1);

                    scheduler.start();
                    [[maybe_unused]] auto receipts = co_await scheduler.execute(blockHeader,
                        fixture.m_transactions | RANGES::views::transform([
                        ](const std::unique_ptr<bcostars::protocol::TransactionImpl>& transaction)
                                                                              -> auto& {
                            return *transaction;
                        }));
                    co_await scheduler.finish(blockHeader, *(fixture.m_cryptoSuite->hashImpl()));
                    co_await scheduler.commit();
                }

                // Check
                auto balances = co_await fixture.balances();
                for (auto&& range : balances | RANGES::views::chunk(2))
                {
                    auto& from = range[0];
                    auto& to = range[1];

                    if (from != singleIssue - singleTransfer * i)
                    {
                        BOOST_THROW_EXCEPTION(
                            std::runtime_error(fmt::format("From balance not equal to expected! {}",
                                from.template convert_to<std::string>())));
                    }

                    if (to != singleIssue + singleTransfer * i)
                    {
                        BOOST_THROW_EXCEPTION(
                            std::runtime_error(fmt::format("To balance not equal to expected! {}",
                                to.template convert_to<std::string>())));
                    }
                }

                co_return;
            }(state));
        },
        fixture.m_scheduler);
}

// static void parallelScheduler(benchmark::State& state) {}
constexpr static bool SERIAL = false;
constexpr static bool PARALLEL = true;
BENCHMARK(issue<SERIAL>)->Arg(1000)->Arg(10000)->Arg(100000);
BENCHMARK(transfer<SERIAL>)->Arg(1000)->Arg(10000)->Arg(100000);
BENCHMARK(issue<PARALLEL>)->Arg(1000)->Arg(10000)->Arg(100000);
BENCHMARK(transfer<PARALLEL>)->Arg(1000)->Arg(10000)->Arg(100000);